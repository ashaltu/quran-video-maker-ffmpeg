#include "background_video_manager.h"
#include "r2_client.h"
#include "cache_utils.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
}

namespace fs = std::filesystem;

namespace BackgroundVideo {

Manager::Manager(const AppConfig& config, const CLIOptions& options)
    : config_(config), options_(options) {
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    tempDir_ = fs::temp_directory_path() / ("qvm_bg_" + std::to_string(timestamp));
    fs::create_directories(tempDir_);
}

double Manager::getVideoDuration(const std::string& path) {
    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, path.c_str(), nullptr, nullptr) != 0) {
        return 0.0;
    }
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        avformat_close_input(&formatContext);
        return 0.0;
    }
    double duration = static_cast<double>(formatContext->duration) / AV_TIME_BASE;
    avformat_close_input(&formatContext);
    return duration;
}

std::string Manager::getCachedVideoPath(const std::string& remoteKey) {
    cacheDir_ = CacheUtils::getCacheRoot() / "backgrounds";
    fs::create_directories(cacheDir_);
    
    std::string safeFilename = remoteKey;
    std::replace(safeFilename.begin(), safeFilename.end(), '/', '_');
    return (cacheDir_ / safeFilename).string();
}

bool Manager::isVideoCached(const std::string& remoteKey) {
    std::string cachedPath = getCachedVideoPath(remoteKey);
    return fs::exists(cachedPath) && fs::file_size(cachedPath) > 0;
}

void Manager::cacheVideo(const std::string& remoteKey, const std::string& localPath) {
    std::string cachePath = getCachedVideoPath(remoteKey);
    if (localPath != cachePath) {
        fs::copy_file(localPath, cachePath, fs::copy_options::overwrite_existing);
    }
}

std::vector<std::string> Manager::listLocalVideos(const std::string& theme) {
    std::vector<std::string> videos;
    fs::path themePath = fs::path(config_.videoSelection.localVideoDirectory) / theme;
    
    if (!fs::exists(themePath) || !fs::is_directory(themePath)) {
        return videos;
    }
    
    for (const auto& entry : fs::directory_iterator(themePath)) {
        if (!entry.is_regular_file()) continue;
        
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || 
            ext == ".mkv" || ext == ".webm") {
            // Return relative path from video directory
            videos.push_back(fs::relative(entry.path(), config_.videoSelection.localVideoDirectory).string());
        }
    }
    
    return videos;
}

std::string Manager::buildFilterComplex(double totalDurationSeconds, 
                                        std::vector<std::string>& outputInputFiles) {
    if (!config_.videoSelection.enableDynamicBackgrounds) {
        return "";  // Use default single input
    }

    try {
        std::cout << "Selecting dynamic background videos..." << std::endl;
        
        // Validate source
        if (config_.videoSelection.useLocalDirectory) {
            if (config_.videoSelection.localVideoDirectory.empty() || 
                !fs::exists(config_.videoSelection.localVideoDirectory)) {
                throw std::runtime_error("Local video directory not found: " + 
                                       config_.videoSelection.localVideoDirectory);
            }
            std::cout << "  Using local directory: " << config_.videoSelection.localVideoDirectory << std::endl;
        } else {
            std::cout << "  Using R2 bucket: " << config_.videoSelection.r2Bucket << std::endl;
        }
        
        // Initialize selector with configured seed
        VideoSelector::Selector selector(
            config_.videoSelection.themeMetadataPath,
            config_.videoSelection.seed
        );
        
        // Get verse range segments with time allocations
        auto verseRangeSegments = selector.getVerseRangeSegments(
            options_.surah, options_.from, options_.to
        );
        
        if (verseRangeSegments.empty()) {
            throw std::runtime_error("No verse range segments found for the specified range");
        }
        
        // Log the verse range segments
        std::cout << "  Verse range segments:" << std::endl;
        for (const auto& seg : verseRangeSegments) {
            std::cout << "    " << seg.rangeKey << " (verses " << seg.startVerse << "-" << seg.endVerse 
                      << ", time " << (seg.startTimeFraction * 100) << "%-" << (seg.endTimeFraction * 100) << "%)"
                      << " themes: [";
            for (size_t i = 0; i < seg.themes.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << seg.themes[i];
            }
            std::cout << "]" << std::endl;
        }
        
        // Calculate absolute time boundaries for each range
        std::map<std::string, double> rangeStartTimes;
        std::map<std::string, double> rangeEndTimes;
        for (const auto& seg : verseRangeSegments) {
            rangeStartTimes[seg.rangeKey] = seg.startTimeFraction * totalDurationSeconds;
            rangeEndTimes[seg.rangeKey] = seg.endTimeFraction * totalDurationSeconds;
        }
        
        // Collect all unique themes
        std::set<std::string> allThemes;
        for (const auto& seg : verseRangeSegments) {
            allThemes.insert(seg.themes.begin(), seg.themes.end());
        }
        
        // Initialize R2 client if using R2
        std::unique_ptr<R2::Client> r2Client;
        if (!config_.videoSelection.useLocalDirectory) {
            R2::R2Config r2Config{
                config_.videoSelection.r2Endpoint,
                config_.videoSelection.r2AccessKey,
                config_.videoSelection.r2SecretKey,
                config_.videoSelection.r2Bucket,
                config_.videoSelection.usePublicBucket
            };
            r2Client = std::make_unique<R2::Client>(r2Config);
        }
        
        // Build video cache for all themes
        std::map<std::string, std::vector<std::string>> themeVideosCache;
        for (const auto& theme : allThemes) {
            try {
                if (config_.videoSelection.useLocalDirectory) {
                    themeVideosCache[theme] = listLocalVideos(theme);
                } else {
                    themeVideosCache[theme] = r2Client->listVideosInTheme(theme);
                }
                
                if (themeVideosCache[theme].empty()) {
                    std::cout << "  Warning: No videos found for theme '" << theme << "'" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error listing videos for theme '" << theme << "': " << e.what() << std::endl;
                themeVideosCache[theme] = {};
            }
        }
        
        // Build playlists for all ranges
        std::cout << "  Building playlists:" << std::endl;
        for (const auto& seg : verseRangeSegments) {
            selector.getOrBuildPlaylist(seg, themeVideosCache, selectionState_);
        }
        
        // Collect video segments
        std::vector<VideoSegment> segments;
        double currentTime = 0.0;
        int segmentCount = 0;
        std::string currentRangeKey;
        const VideoSelector::VerseRangeSegment* currentRange = nullptr;
        
        // Calculate reasonable segment limit based on duration
        int maxSegments = std::max(500, static_cast<int>(totalDurationSeconds / 5.0));
        
        while (currentTime < totalDurationSeconds && segmentCount < maxSegments) {
            segmentCount++;
            
            double timeFraction = currentTime / totalDurationSeconds;
            
            // Get the appropriate verse range segment for this time position
            const auto* newRange = selector.getRangeForTimePosition(verseRangeSegments, timeFraction);
            if (!newRange) break;
            
            // Check if we changed ranges
            if (currentRange != newRange) {
                if (currentRange != nullptr) {
                    std::cout << "  --- Transitioning from " << currentRange->rangeKey 
                              << " to " << newRange->rangeKey << " ---" << std::endl;
                }
                currentRange = newRange;
                currentRangeKey = newRange->rangeKey;
            }
            
            // Calculate time remaining for this range
            double rangeEndTime = rangeEndTimes[currentRangeKey];
            double timeRemainingInRange = rangeEndTime - currentTime;
            
            // Get next video from the range's playlist
            VideoSelector::PlaylistEntry entry;
            try {
                entry = selector.getNextVideoForRange(currentRangeKey, selectionState_);
            } catch (const std::exception& e) {
                std::cerr << "  Error getting next video: " << e.what() << std::endl;
                break;
            }
            
            std::cout << "  Segment " << segmentCount 
                      << " [" << currentRangeKey << "]"
                      << " - theme: " << entry.theme 
                      << ", video: " << fs::path(entry.videoKey).filename().string();
            
            // Get the video (download from R2 or use local)
            std::string localPath;
            
            if (config_.videoSelection.useLocalDirectory) {
                // Local directory - construct full path
                localPath = (fs::path(config_.videoSelection.localVideoDirectory) / entry.videoKey).string();
                if (!fs::exists(localPath)) {
                    std::cerr << " (file not found)" << std::endl;
                    continue;
                }
            } else {
                // R2 - check cache first, then download
                if (isVideoCached(entry.videoKey)) {
                    localPath = getCachedVideoPath(entry.videoKey);
                    std::cout << " (cached)";
                } else {
                    fs::path tempPath = tempDir_ / fs::path(entry.videoKey).filename();
                    try {
                        localPath = r2Client->downloadVideo(entry.videoKey, tempPath);
                        cacheVideo(entry.videoKey, localPath);
                        tempFiles_.push_back(tempPath);
                    } catch (const std::exception& e) {
                        std::cerr << " (download failed: " << e.what() << ")" << std::endl;
                        continue;
                    }
                }
            }
            
            // Get video duration
            double duration = getVideoDuration(localPath);
            if (duration <= 0) {
                std::cerr << " (invalid duration)" << std::endl;
                continue;
            }
            
            std::cout << ", duration: " << duration << "s";
            
            // Build segment info
            VideoSegment segment;
            segment.path = localPath;
            segment.theme = entry.theme;
            segment.duration = duration;
            segment.isLocal = true;
            segment.needsTrim = false;
            segment.trimmedDuration = duration;
            
            // Check if this video would extend beyond the current range
            if (currentTime + duration > rangeEndTime && timeRemainingInRange > 0.5) {
                // This video would cross into the next range - trim it
                segment.needsTrim = true;
                segment.trimmedDuration = timeRemainingInRange;
                std::cout << " (trimming to " << segment.trimmedDuration << "s to fit range)";
            }
            
            // Also check if it would exceed total duration
            if (currentTime + segment.trimmedDuration > totalDurationSeconds) {
                segment.needsTrim = true;
                segment.trimmedDuration = totalDurationSeconds - currentTime;
                std::cout << " (trimming to " << segment.trimmedDuration << "s to end)";
            }
            
            std::cout << std::endl;
            
            segments.push_back(segment);
            outputInputFiles.push_back(localPath);
            currentTime += segment.trimmedDuration;
        }
        
        if (segments.empty()) {
            std::cerr << "Warning: No video segments collected" << std::endl;
            return "";
        }
        
        std::cout << "  Collected " << segments.size() << " segments, total duration: " 
                  << currentTime << " seconds" << std::endl;
        
        // Build concat filter
        std::ostringstream filter;
        
        // First, scale and trim all inputs
        for (size_t i = 0; i < segments.size(); ++i) {
            filter << "[" << i << ":v]";
            
            // Trim if needed
            if (segments[i].needsTrim) {
                filter << "trim=duration=" << segments[i].trimmedDuration << ",setpts=PTS-STARTPTS,";
            }
            
            // Scale to configured dimensions and normalize parameters
            filter << "scale=" << config_.width << ":" << config_.height 
                   << ",fps=" << config_.fps
                   << ",format=" << config_.pixelFormat
                   << ",setsar=1[v" << i << "]; ";
        }
        
        // Then concat them
        for (size_t i = 0; i < segments.size(); ++i) {
            filter << "[v" << i << "]";
        }
        filter << "concat=n=" << segments.size() << ":v=1:a=0[bg]; ";
        filter << "[bg]setpts=PTS-STARTPTS";
        
        return filter.str();
        
    } catch (const std::exception& e) {
        std::cerr << "Warning: Dynamic background selection failed: " << e.what() 
                  << ", using default background" << std::endl;
        return "";
    }
}

void Manager::cleanup() {
    for (const auto& file : tempFiles_) {
        std::error_code ec;
        fs::remove(file, ec);
    }
    if (fs::exists(tempDir_)) {
        std::error_code ec;
        fs::remove_all(tempDir_, ec);
    }
}

} // namespace BackgroundVideo