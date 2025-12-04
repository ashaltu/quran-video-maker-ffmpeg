#include "video_selector.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iostream>

using json = nlohmann::json;

namespace VideoSelector {

SeededRandom::SeededRandom(unsigned int seed) : gen(seed) {}

int SeededRandom::nextInt(int min, int max) {
    if (min >= max) return min;
    std::uniform_int_distribution<> dis(min, max - 1);
    return dis(gen);
}

void SeededRandom::shuffle(std::vector<std::pair<std::string, std::string>>& items) {
    for (size_t i = items.size() - 1; i > 0; --i) {
        size_t j = nextInt(0, i + 1);
        std::swap(items[i], items[j]);
    }
}

Selector::Selector(const std::string& metadataPath, unsigned int seed)
    : random(seed) {
    std::ifstream file(metadataPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open theme metadata: " + metadataPath);
    }
    file >> metadata;
}

std::pair<int, int> Selector::findRangeBoundsForVerse(int surah, int verse) {
    std::string surahKey = std::to_string(surah);
    if (!metadata.contains(surahKey)) {
        return {-1, -1};
    }
    
    const auto& surahData = metadata[surahKey];
    for (auto it = surahData.begin(); it != surahData.end(); ++it) {
        const std::string& range = it.key();
        size_t dashPos = range.find('-');
        if (dashPos == std::string::npos) continue;
        
        int start = std::stoi(range.substr(0, dashPos));
        int end = std::stoi(range.substr(dashPos + 1));
        
        if (verse >= start && verse <= end) {
            return {start, end};
        }
    }
    
    return {-1, -1};
}

std::vector<std::string> Selector::findRangeForVerse(int surah, int verse) {
    std::string surahKey = std::to_string(surah);
    if (!metadata.contains(surahKey)) {
        return {};
    }
    
    const auto& surahData = metadata[surahKey];
    for (auto it = surahData.begin(); it != surahData.end(); ++it) {
        const std::string& range = it.key();
        size_t dashPos = range.find('-');
        if (dashPos == std::string::npos) continue;
        
        int start = std::stoi(range.substr(0, dashPos));
        int end = std::stoi(range.substr(dashPos + 1));
        
        if (verse >= start && verse <= end) {
            return it.value().get<std::vector<std::string>>();
        }
    }
    
    return {};
}

std::vector<VerseRangeSegment> Selector::getVerseRangeSegments(int surah, int from, int to) {
    std::vector<VerseRangeSegment> segments;
    
    // Find all unique verse ranges that overlap with our requested range
    std::map<std::string, VerseRangeSegment> rangeMap;
    
    for (int verse = from; verse <= to; ++verse) {
        auto bounds = findRangeBoundsForVerse(surah, verse);
        if (bounds.first < 0) continue;
        
        std::string rangeKey = std::to_string(surah) + ":" + 
                               std::to_string(bounds.first) + "-" + 
                               std::to_string(bounds.second);
        
        if (rangeMap.find(rangeKey) == rangeMap.end()) {
            VerseRangeSegment segment;
            segment.rangeKey = rangeKey;
            // Clamp to our requested range
            segment.startVerse = std::max(bounds.first, from);
            segment.endVerse = std::min(bounds.second, to);
            segment.themes = findRangeForVerse(surah, verse);
            segment.startTimeFraction = 0.0;
            segment.endTimeFraction = 0.0; 
            rangeMap[rangeKey] = segment;
        } else {
            // Update the end verse if needed
            rangeMap[rangeKey].endVerse = std::max(rangeMap[rangeKey].endVerse, 
                                                    std::min(bounds.second, to));
        }
    }
    
    // Convert to vector and sort by start verse
    for (auto& [key, segment] : rangeMap) {
        segments.push_back(segment);
    }
    
    std::sort(segments.begin(), segments.end(), 
              [](const VerseRangeSegment& a, const VerseRangeSegment& b) {
                  return a.startVerse < b.startVerse;
              });
    
    // Calculate time fractions based on verse counts
    int totalVerses = to - from + 1;
    double currentFraction = 0.0;
    
    for (auto& segment : segments) {
        int verseCount = segment.endVerse - segment.startVerse + 1;
        double fraction = static_cast<double>(verseCount) / totalVerses;
        
        segment.startTimeFraction = currentFraction;
        segment.endTimeFraction = currentFraction + fraction;
        currentFraction += fraction;
    }
    
    // Ensure last segment ends at exactly 1.0
    if (!segments.empty()) {
        segments.back().endTimeFraction = 1.0;
    }
    
    return segments;
}

const VerseRangeSegment* Selector::getRangeForTimePosition(
    const std::vector<VerseRangeSegment>& segments,
    double timeFraction) {
    
    for (const auto& segment : segments) {
        if (timeFraction >= segment.startTimeFraction && 
            timeFraction < segment.endTimeFraction) {
            return &segment;
        }
    }
    
    // If at exactly 1.0 or beyond, return the last segment
    if (!segments.empty() && timeFraction >= segments.back().startTimeFraction) {
        return &segments.back();
    }
    
    return nullptr;
}

std::vector<PlaylistEntry> Selector::buildPlaylist(
    const std::vector<std::string>& themes,
    const std::map<std::string, std::vector<std::string>>& themeVideosCache) {
    
    // Build lists of videos per theme (only themes with videos)
    std::vector<std::pair<std::string, std::vector<std::string>>> themeVideos;
    for (const auto& theme : themes) {
        auto it = themeVideosCache.find(theme);
        if (it != themeVideosCache.end() && !it->second.empty()) {
            themeVideos.push_back({theme, it->second});
        }
    }
    
    if (themeVideos.empty()) {
        return {};
    }
    
    // Shuffle the themes order
    std::vector<std::pair<std::string, std::string>> themePairs;
    for (const auto& [theme, _] : themeVideos) {
        themePairs.push_back({theme, ""});
    }
    random.shuffle(themePairs);
    
    // Rebuild themeVideos in shuffled order
    std::vector<std::pair<std::string, std::vector<std::string>>> shuffledThemeVideos;
    for (const auto& [theme, _] : themePairs) {
        for (const auto& tv : themeVideos) {
            if (tv.first == theme) {
                shuffledThemeVideos.push_back(tv);
                break;
            }
        }
    }
    themeVideos = shuffledThemeVideos;
    
    // Shuffle videos within each theme
    for (auto& [theme, videos] : themeVideos) {
        std::vector<std::pair<std::string, std::string>> videoPairs;
        for (const auto& v : videos) {
            videoPairs.push_back({v, ""});
        }
        random.shuffle(videoPairs);
        videos.clear();
        for (const auto& [v, _] : videoPairs) {
            videos.push_back(v);
        }
    }
    
    // Interleave: cycle through themes, taking one video from each in turn
    std::vector<PlaylistEntry> playlist;
    std::vector<size_t> indices(themeVideos.size(), 0);
    
    bool hasMore = true;
    while (hasMore) {
        hasMore = false;
        for (size_t t = 0; t < themeVideos.size(); ++t) {
            const auto& [theme, videos] = themeVideos[t];
            if (indices[t] < videos.size()) {
                PlaylistEntry entry;
                entry.theme = theme;
                entry.videoKey = videos[indices[t]];
                playlist.push_back(entry);
                indices[t]++;
                if (indices[t] < videos.size()) {
                    hasMore = true;
                }
            }
        }
        // Check if any theme still has videos
        if (!hasMore) {
            for (size_t t = 0; t < themeVideos.size(); ++t) {
                if (indices[t] < themeVideos[t].second.size()) {
                    hasMore = true;
                    break;
                }
            }
        }
    }
    
    return playlist;
}

const std::vector<PlaylistEntry>& Selector::getOrBuildPlaylist(
    const VerseRangeSegment& range,
    const std::map<std::string, std::vector<std::string>>& themeVideosCache,
    SelectionState& state) {
    
    auto it = state.rangePlaylists.find(range.rangeKey);
    if (it == state.rangePlaylists.end()) {
        // Build playlist for this range
        auto playlist = buildPlaylist(range.themes, themeVideosCache);
        state.rangePlaylists[range.rangeKey] = std::move(playlist);
        state.rangePlaylistIndices[range.rangeKey] = 0;
        
        // Log the playlist
        std::cout << "    Built playlist for " << range.rangeKey << ": ";
        const auto& pl = state.rangePlaylists[range.rangeKey];
        for (size_t i = 0; i < pl.size(); ++i) {
            if (i > 0) std::cout << " -> ";
            std::cout << pl[i].theme << "/" << pl[i].videoKey.substr(pl[i].videoKey.rfind('/') + 1);
        }
        std::cout << std::endl;
    }
    
    return state.rangePlaylists[range.rangeKey];
}

PlaylistEntry Selector::getNextVideoForRange(
    const std::string& rangeKey,
    SelectionState& state) {
    
    auto playlistIt = state.rangePlaylists.find(rangeKey);
    if (playlistIt == state.rangePlaylists.end() || playlistIt->second.empty()) {
        throw std::runtime_error("No playlist found for range: " + rangeKey);
    }
    
    const auto& playlist = playlistIt->second;
    size_t& index = state.rangePlaylistIndices[rangeKey];
    
    // Get current entry
    const PlaylistEntry& entry = playlist[index];
    
    // Advance index (wrap around)
    index = (index + 1) % playlist.size();
    
    // Log if we wrapped around
    if (index == 0) {
        std::cout << "    Playlist for " << rangeKey << " cycling back to start" << std::endl;
    }
    
    return entry;
}

} // namespace VideoSelector