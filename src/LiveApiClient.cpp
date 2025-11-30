#include "LiveApiClient.h"
#include "quran_data.h"
#include "timing_parser.h"
#include "cache_utils.h"
#include "recitation_utils.h"
#include "audio/custom_audio_processor.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <optional>
#include <cstdlib>
#include <limits>
#include <cctype>

namespace fs = std::filesystem;
using json = nlohmann::json;

    // GAPPED MODE: Fetch individual ayah data
    VerseData fetch_single_verse_gapped(int surah, int verseNum, const AppConfig& config, bool useCache, const fs::path& audioDir) {
        std::string verseKey = std::to_string(surah) + ":" + std::to_string(verseNum);
        fs::path cachePath = CacheUtils::getCacheRoot() / (verseKey + "_r" + std::to_string(config.reciterId) + "_t" + std::to_string(config.translationId) + "_gapped.json");

        if (useCache && fs::exists(cachePath)) {
            try {
                std::ifstream f(cachePath);
                json data = json::parse(f);
                std::cout << "  - Using cached data for " << verseKey << std::endl;
                return {
                    data.at("verseKey"), data.at("text"), data.at("translation"),
                    data.at("audioUrl"), data.at("durationInSeconds"), data.at("localAudioPath"),
                    data.value("timestampFromMs", 0), data.value("timestampToMs", 0), {}
                };
            } catch (const json::exception&) {
                std::cout << "  - Cache invalid for " << verseKey << ", re-fetching." << std::endl;
            }
        }

        VerseData result;
        result.verseKey = verseKey;
        result.text = "";

        // Load translation
        try {
            result.translation = CacheUtils::getTranslationText(config.translationId, verseKey);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not load translation for " << verseKey << ": " << e.what() << std::endl;
            result.translation.clear();
        }

        const json& audioData = CacheUtils::getReciterAudioData(config.reciterId);
        auto verseAudioIt = audioData.find(verseKey);
        if (verseAudioIt == audioData.end() || !verseAudioIt->is_object()) {
            throw std::runtime_error("Verse not found in audio JSON: " + verseKey);
        }
        const auto& verseAudio = *verseAudioIt;
        result.audioUrl = verseAudio.value("audio_url", "");
        if (result.audioUrl.empty()) {
            throw std::runtime_error("Audio URL missing for verse " + verseKey);
        }

        std::string sanitized = CacheUtils::sanitizeLabel(verseKey + "_r" + std::to_string(config.reciterId) + ".mp3");
        fs::path audioPath = useCache ? CacheUtils::buildCachedAudioPath(sanitized)
                                      : (audioDir / sanitized);
        if (!useCache || !CacheUtils::fileIsValid(audioPath)) {
            if (!CacheUtils::downloadFileWithRetry(result.audioUrl, audioPath)) {
                throw std::runtime_error("Failed to download audio for " + verseKey + " from " + result.audioUrl);
            }
        }
        result.localAudioPath = audioPath.string();

        result.durationInSeconds = Audio::CustomAudioProcessor::probeDuration(result.localAudioPath);
        if (result.durationInSeconds <= 0.0 && verseAudio.contains("duration") && !verseAudio["duration"].is_null()) {
            try {
                result.durationInSeconds = verseAudio["duration"].get<double>();
            } catch (...) {
                // ignore if unparsable
            }
        }
        if (result.durationInSeconds <= 0.0) {
            std::cerr << "\nWarning: Could not determine duration for " << verseKey << ".\n";
        }

        result.absoluteTimestampFromMs = result.timestampFromMs;
        result.absoluteTimestampToMs = result.timestampToMs;
        result.fromCustomAudio = false;
        result.sourceAudioPath = result.localAudioPath;

        // Save to cache
        if (useCache) {
            fs::create_directories(CacheUtils::getCacheRoot());
            json cacheData = {
                {"verseKey", result.verseKey}, {"text", result.text}, {"translation", result.translation},
                {"audioUrl", result.audioUrl}, {"durationInSeconds", result.durationInSeconds},
                {"localAudioPath", result.localAudioPath}
            };
            std::ofstream o(cachePath);
            o << std::setw(4) << cacheData << std::endl;
        }

        return result;
    }

    // GAPLESS MODE: Fetch verse data with timing from surah audio or custom source
    std::vector<VerseData> fetch_verses_gapless(int surah,
                                                int from,
                                                int to,
                                                const AppConfig& config,
                                                bool useCache,
                                                const fs::path& audioDir,
                                                const CLIOptions& options,
                                                std::optional<TimingEntry>* customBismillahTiming = nullptr) {
        std::cout << "  - Using GAPLESS mode (surah-by-surah)" << std::endl;
        
        std::string localAudioPath;
        std::map<std::string, TimingEntry> timings;
        std::vector<TimingEntry> sequentialTimings;
        std::map<int, std::deque<TimingEntry>> verseBuckets;
        std::optional<TimingEntry> detectedCustomBismillah;
        
        // Check if using custom recitation
        if (!options.customAudioPath.empty() && !options.customTimingFile.empty()) {
            std::cout << "  - Using CUSTOM recitation" << std::endl;
            
            // Parse timing file
            auto parsedTimings = TimingParser::parseTimingFile(options.customTimingFile);
            timings = std::move(parsedTimings.byKey);
            sequentialTimings = std::move(parsedTimings.ordered);
            verseBuckets = std::move(parsedTimings.byVerseNumber);

            auto remove_from_buckets = [&](const TimingEntry& entry) {
                auto bucketIt = verseBuckets.find(entry.verseNumber);
                if (bucketIt == verseBuckets.end()) return;
                auto& dq = bucketIt->second;
                for (auto it = dq.begin(); it != dq.end(); ++it) {
                    if (it->sequentialIndex == entry.sequentialIndex) {
                        dq.erase(it);
                        break;
                    }
                }
                if (dq.empty()) verseBuckets.erase(bucketIt);
            };

            for (auto it = sequentialTimings.begin(); it != sequentialTimings.end();) {
                if (it->isBismillah) {
                    if (!detectedCustomBismillah) {
                        detectedCustomBismillah = *it;
                    }
                    timings.erase(it->verseKey);
                    remove_from_buckets(*it);
                    it = sequentialTimings.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Download or copy audio file
            if (options.customAudioPath.find("http://") == 0 || options.customAudioPath.find("https://") == 0) {
                // Download from URL
                localAudioPath = (audioDir / ("custom_surah_" + std::to_string(surah) + ".mp3")).string();
                if (!useCache || !fs::exists(localAudioPath)) {
                    std::cout << "  - Downloading custom audio from " << options.customAudioPath << std::endl;
                    if (!CacheUtils::downloadFileWithRetry(options.customAudioPath, localAudioPath)) {
                        throw std::runtime_error("Failed to download custom audio from " + options.customAudioPath);
                    }
                } else {
                    std::cout << "  - Using cached custom audio" << std::endl;
                }
            } else {
                // Use local file path
                localAudioPath = options.customAudioPath;
                if (!fs::exists(localAudioPath))
                    throw std::runtime_error("Custom audio file not found: " + localAudioPath);
            }
        } else {
            // Use standard reciter data
            auto recDirIt = QuranData::gaplessReciterDirs.find(config.reciterId);
            if (recDirIt == QuranData::gaplessReciterDirs.end())
                throw std::runtime_error("Reciter ID " + std::to_string(config.reciterId) + " not available for gapless mode");

            fs::path reciterDir = CacheUtils::resolveDataPath(recDirIt->second);
            fs::path surahJsonPath = reciterDir / "surah.json";
            fs::path segmentsJsonPath = reciterDir / "segments.json";

            if (!fs::exists(surahJsonPath) || !fs::exists(segmentsJsonPath))
                throw std::runtime_error("Missing surah.json or segments.json for reciter in " + reciterDir.string());

            // Load surah metadata (audio URL)
            std::ifstream surahFile(surahJsonPath);
            json surahData = json::parse(surahFile);
            std::string surahKey = std::to_string(surah);
            if (!surahData.contains(surahKey))
                throw std::runtime_error("Surah " + surahKey + " not found in surah.json");

            std::string audioUrl = surahData[surahKey]["audio_url"].get<std::string>();
            
            // Download the full surah audio
            localAudioPath = (audioDir / ("surah_" + std::to_string(surah) + "_r" + std::to_string(config.reciterId) + ".mp3")).string();
            
            if (!useCache || !fs::exists(localAudioPath)) {
                std::cout << "  - Downloading full surah audio from " << audioUrl << std::endl;
                if (!CacheUtils::downloadFileWithRetry(audioUrl, localAudioPath)) {
                    throw std::runtime_error("Failed to download surah audio from " + audioUrl);
                }
            } else {
                std::cout << "  - Using cached surah audio" << std::endl;
            }

            // Load segments (timing information)
            std::ifstream segmentsFile(segmentsJsonPath);
            json segmentsData = json::parse(segmentsFile);
            
            // Convert segments to timing map
            for (int verseNum = from; verseNum <= to; ++verseNum) {
                std::string verseKey = std::to_string(surah) + ":" + std::to_string(verseNum);
                if (segmentsData.contains(verseKey)) {
                    const auto& verseSegment = segmentsData[verseKey];
                    TimingEntry entry;
                    entry.verseKey = verseKey;
                    entry.startMs = verseSegment["timestamp_from"].get<int>();
                    entry.endMs = verseSegment["timestamp_to"].get<int>();
                    timings[verseKey] = entry;
                }
            }
        }

        // Align sequential timings with requested starting verse if possible
        if (!sequentialTimings.empty() && options.from > 1) {
            auto startIt = std::find_if(sequentialTimings.begin(), sequentialTimings.end(),
                [&](const TimingEntry& entry) { return entry.verseNumber == options.from; });
            if (startIt != sequentialTimings.end()) {
                sequentialTimings.erase(sequentialTimings.begin(), startIt);
            } else if (sequentialTimings.size() > static_cast<size_t>(options.from - 1)) {
                sequentialTimings.erase(sequentialTimings.begin(),
                                        sequentialTimings.begin() + (options.from - 1));
            }
        }

        const json& translations = CacheUtils::getTranslationData(config.translationId);

        auto buildVerseFromTiming = [&](const TimingEntry& timing) {
            VerseData verse;
            std::string normalizedKey = timing.verseKey.rfind("SURAH:", 0) == 0
                ? std::to_string(surah) + ":" + std::to_string(timing.verseNumber)
                : timing.verseKey;
            verse.verseKey = normalizedKey;
            verse.audioUrl = "";
            verse.localAudioPath = localAudioPath;
            verse.timestampFromMs = timing.startMs;
            verse.timestampToMs = timing.endMs;
            verse.durationInSeconds = (verse.timestampToMs - verse.timestampFromMs) / 1000.0;
            verse.absoluteTimestampFromMs = verse.timestampFromMs;
            verse.absoluteTimestampToMs = verse.timestampToMs;
            verse.fromCustomAudio = !options.customAudioPath.empty();
            verse.sourceAudioPath = localAudioPath;

            auto transIt = translations.find(normalizedKey);
            if (transIt != translations.end() && transIt->is_object()) {
                auto textIt = transIt->find("t");
                verse.translation = (textIt != transIt->end() && textIt->is_string())
                    ? textIt->get<std::string>()
                    : "";
            } else {
                verse.translation = "";
            }
            verse.text = "";
            return verse;
        };

        std::vector<VerseData> results;
        bool builtFromTimeline = false;
        if (!options.customTimingFile.empty()) {
            for (const auto& timing : sequentialTimings) {
                if (timing.verseNumber >= from && timing.verseNumber <= to) {
                    results.push_back(buildVerseFromTiming(timing));
                    builtFromTimeline = true;
                }
            }
        }

        if (!builtFromTimeline) {
            size_t sequentialCursor = 0;
            for (int verseNum = from; verseNum <= to; ++verseNum) {
                std::string verseKey = std::to_string(surah) + ":" + std::to_string(verseNum);
                std::string timingKey = options.customTimingFile.empty() ? verseKey : ("SURAH:" + std::to_string(verseNum));
                
                const TimingEntry* timingPtr = nullptr;
                TimingEntry tempTiming;
                bool usedSequentialFallback = false;
                auto directIt = timings.find(verseKey);
                if (directIt != timings.end()) {
                    timingPtr = &directIt->second;
                } else if (timings.count(timingKey)) {
                    timingPtr = &timings[timingKey];
                } else {
                    auto bucketIt = verseBuckets.find(verseNum);
                    if (bucketIt != verseBuckets.end() && !bucketIt->second.empty()) {
                        tempTiming = bucketIt->second.front();
                        bucketIt->second.pop_front();
                        if (bucketIt->second.empty()) verseBuckets.erase(bucketIt);
                        timingPtr = &tempTiming;
                    } else if (!sequentialTimings.empty() && sequentialCursor < sequentialTimings.size()) {
                        tempTiming = sequentialTimings[sequentialCursor];
                        timingPtr = &tempTiming;
                        usedSequentialFallback = true;
                        std::cerr << "Warning: Verse " << verseKey
                                  << " missing explicit timing entry; falling back to sequential ordering from custom timing file."
                                  << std::endl;
                    }
                }

                if (!timingPtr)
                    throw std::runtime_error("Verse " + verseKey + " not found in timing data");

                if (usedSequentialFallback) {
                    sequentialCursor++;
                }

                results.push_back(buildVerseFromTiming(*timingPtr));
            }
        }

        if (customBismillahTiming && detectedCustomBismillah) {
            *customBismillahTiming = detectedCustomBismillah;
        }

        RecitationUtils::normalizeGaplessTimings(results);
        return results;
    }

void pop_back_utf8(std::string& s) {
    if (s.empty()) return;
    size_t i = s.size() - 1;
    while (i > 0 && (s[i] & 0xC0) == 0x80) {
        --i;
    }
    s.erase(i);
}

void trim_last_word(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    while (!s.empty() && !std::isspace(static_cast<unsigned char>(s.back()))) {
        pop_back_utf8(s);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
}

std::vector<VerseData> LiveApiClient::fetchQuranData(const CLIOptions& options, const AppConfig& config) {
    std::cout << "Fetching data for Surah " << options.surah << ", verses " << options.from << "-" << options.to << "..." << std::endl;
    
    auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path audioDir = fs::temp_directory_path() / ("quran_video_audio_" + std::to_string(uniqueSuffix));
    std::error_code ec;
    fs::create_directories(audioDir, ec);

    std::vector<VerseData> results;
    std::optional<TimingEntry> customBismillahTiming;
    
    // Choose mode based on config
    if (config.recitationMode == RecitationMode::GAPLESS) {
        results = fetch_verses_gapless(options.surah, options.from, options.to, config, !options.noCache, audioDir, options, &customBismillahTiming);
    } else {
        // GAPPED mode - parallel fetch
        std::vector<std::future<VerseData>> futures;
        for (int i = options.from; i <= options.to; ++i) {
            futures.push_back(std::async(std::launch::async, fetch_single_verse_gapped, options.surah, i, config, !options.noCache, audioDir));
        }

        results.reserve(futures.size());
        for (auto& fut : futures) {
            results.push_back(fut.get());
        }
        
        std::sort(results.begin(), results.end(), [](const VerseData& a, const VerseData& b) {
            int verseA = std::stoi(a.verseKey.substr(a.verseKey.find(':') + 1));
            int verseB = std::stoi(b.verseKey.substr(b.verseKey.find(':') + 1));
            return verseA < verseB;
        });
    }

    // Load QPC Uthmani text for all verses
    std::ifstream file(config.quranWordByWordPath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << config.quranWordByWordPath << "\n";
        return results;
    }
    json quranData;
    file >> quranData;

    // Add Bismillah if needed
    if (options.surah != 1 && options.surah != 9) {
        if (config.recitationMode == RecitationMode::GAPLESS && customBismillahTiming && !options.customAudioPath.empty()) {
            if (!results.empty()) {
                results.insert(results.begin(), RecitationUtils::buildBismillahFromTiming(*customBismillahTiming, config, results.front().localAudioPath));
            }
        } else if (config.recitationMode == RecitationMode::GAPLESS) {
            auto bismillahVerses = fetch_verses_gapless(1, 1, 1, config, !options.noCache, audioDir, options, nullptr);
            if (!bismillahVerses.empty()) {
                results.insert(results.begin(), bismillahVerses[0]);
            }
        } else {
            results.insert(results.begin(), fetch_single_verse_gapped(1, 1, config, !options.noCache, audioDir));
        }
    }

    // Fill in QPC Arabic text
    for (auto& verse : results) {
        std::string keyPrefix = verse.verseKey + ":";
        std::vector<std::pair<int, std::string>> words;
        for (auto it = quranData.begin(); it != quranData.end(); ++it) {
            if (it.key().rfind(keyPrefix, 0) == 0) {
                auto parts = it.key().substr(keyPrefix.size());
                int wordIndex = std::stoi(parts);
                words.emplace_back(wordIndex, it.value().value("text", ""));
            }
        }
        std::sort(words.begin(), words.end(),
                [](auto& a, auto& b) { return a.first < b.first; });

        std::string text;
        for (auto& w : words)
            text += w.second + " ";

        if (!text.empty())
            verse.text = text;
    }

    // Remove last word from Bismillah if it's not Surah 1 or 9
    if (options.surah != 1 && options.surah != 9) {
        if (!results.empty() && !results[0].text.empty()) {
            trim_last_word(results[0].text);
        }
    }

    bool hasCustomRange = config.recitationMode == RecitationMode::GAPLESS &&
                          !options.customAudioPath.empty();
    bool shouldSpliceCustomClip = hasCustomRange && options.from > 1;
    if (shouldSpliceCustomClip) {
        Audio::CustomAudioProcessor::spliceRange(results, options, audioDir);
    } else if (hasCustomRange) {
        for (auto& verse : results) {
            verse.fromCustomAudio = false;
        }
    }

    return results;
}
