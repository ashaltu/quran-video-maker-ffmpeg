#include "background_video_manager.h"
#include "r2_client.h"
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

std::vector<VideoSegment> Manager::collectVideoSegments(double targetDuration) {
    std::vector<VideoSegment> segments;
    double totalDuration = 0.0;
    
    std::cout << "  Target duration: " << targetDuration << " seconds" << std::endl;
    std::cout << "  Collecting video segments..." << std::endl;
    
    // Initialize selector
    VideoSelector::Selector selector(
        config_.videoSelection.themeMetadataPath,
        config_.videoSelection.seed
    );
    
    // Get verse range segments with time allocations
    auto verseRangeSegments = selector.getVerseRangeSegments(
        options_.surah, 
        options_.from, 
        options_.to
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
    std::map<std::string, double> rangeEndTimes;
    for (const auto& seg : verseRangeSegments) {
        rangeEndTimes[seg.rangeKey] = seg.endTimeFraction * targetDuration;
    }
    
    // Collect all unique themes across all ranges
    std::set<std::string> allThemes;
    for (const auto& seg : verseRangeSegments) {
        allThemes.insert(seg.themes.begin(), seg.themes.end());
    }
    
    std::cout << "  All themes: ";
    bool first = true;
    for (const auto& theme : allThemes) {
        if (!first) std::cout << ", ";
        std::cout << theme;
        first = false;
    }
    std::cout << std::endl;
    
    // Initialize R2 client
    R2::R2Config r2Config{
        config_.videoSelection.r2Endpoint,
        config_.videoSelection.r2AccessKey,
        config_.videoSelection.r2SecretKey,
        config_.videoSelection.r2Bucket,
        config_.videoSelection.usePublicBucket
    };
    R2::Client r2Client(r2Config);
    
    // Pre-fetch all available videos for all themes
    std::map<std::string, std::vector<std::string>> themeVideosCache;
    for (const auto& theme : allThemes) {
        try {
            themeVideosCache[theme] = r2Client.listVideosInTheme(theme);
            if (themeVideosCache[theme].empty()) {
                std::cout << "  Warning: No videos found for theme '" << theme << "'" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  Error listing videos for theme '" << theme << "': " << e.what() << std::endl;
            themeVideosCache[theme] = {};
        }
    }
    
    // Pre-build playlists for all ranges
    std::cout << "  Building playlists:" << std::endl;
    for (const auto& seg : verseRangeSegments) {
        selector.getOrBuildPlaylist(seg, themeVideosCache, selectionState_);
    }
    
    // Track current range
    std::string currentRangeKey;
    const VideoSelector::VerseRangeSegment* currentRange = nullptr;
    
    // Collect segments until we meet or exceed the target duration
    int segmentCount = 0;
    
    while (totalDuration < targetDuration) {
        segmentCount++;
        
        // Calculate current time fraction
        double timeFraction = totalDuration / targetDuration;
        
        // Get the appropriate verse range segment for this time position
        const auto* newRange = selector.getRangeForTimePosition(verseRangeSegments, timeFraction);
        if (!newRange) {
            std::cerr << "  Error: No range found for time fraction " << timeFraction << std::endl;
            break;
        }
        
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
        double timeRemainingInRange = rangeEndTime - totalDuration;
        
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
        
        // Download the video
        fs::path localPath = tempDir_ / (std::to_string(segmentCount) + "_" + 
                                         fs::path(entry.videoKey).filename().string());
        std::string downloadedPath;
        try {
            downloadedPath = r2Client.downloadVideo(entry.videoKey, localPath);
            tempFiles_.push_back(localPath);
        } catch (const std::exception& e) {
            std::cerr << " (download failed: " << e.what() << ")" << std::endl;
            continue;
        }
        
        // Get video duration
        double videoDuration = getVideoDuration(downloadedPath);
        if (videoDuration <= 0) {
            std::cerr << " (invalid duration, skipping)" << std::endl;
            continue;
        }
        
        std::cout << ", duration: " << videoDuration << "s";
        
        // Check if this video would extend beyond the current range
        VideoSegment segment;
        segment.path = downloadedPath;
        segment.theme = entry.theme;
        segment.duration = videoDuration;
        segment.isLocal = true;
        segment.needsTrim = false;
        segment.trimmedDuration = videoDuration;
        
        // Check if we need to trim to fit the range boundary
        if (totalDuration + videoDuration > rangeEndTime && 
            timeRemainingInRange > 0.5) {  // Only trim if there's meaningful time left
            
            // This video would cross into the next range - trim it
            segment.needsTrim = true;
            segment.trimmedDuration = timeRemainingInRange;
            std::cout << " (trimming to " << segment.trimmedDuration << "s to end range)";
        }
        
        std::cout << std::endl;
        
        segments.push_back(segment);
        totalDuration += segment.trimmedDuration;
        
        // Safety limit to prevent infinite loops
        if (segmentCount > 200) {
            std::cerr << "  Warning: Reached segment limit, stopping collection" << std::endl;
            break;
        }
    }
    
    std::cout << "  Collected " << segments.size() << " segments, total duration: " 
              << totalDuration << " seconds" << std::endl;
    
    return segments;
}

std::string Manager::stitchVideos(const std::vector<VideoSegment>& segments) {
    if (segments.empty()) {
        throw std::runtime_error("No video segments to stitch");
    }
    
    if (segments.size() == 1 && !segments[0].needsTrim) {
        std::cout << "  Single segment, normalizing for consistency..." << std::endl;
    } else {
        std::cout << "  Stitching " << segments.size() << " video segments..." << std::endl;
    }
    
    std::cout << "  Re-encoding segments to ensure compatibility..." << std::endl;
    
    // First pass: re-encode all segments (with trimming if needed)
    std::vector<std::string> normalizedSegments;
    for (size_t i = 0; i < segments.size(); ++i) {
        fs::path normalizedPath = tempDir_ / ("normalized_" + std::to_string(i) + ".mp4");
        tempFiles_.push_back(normalizedPath);
        
        std::ostringstream cmd;
        cmd << "ffmpeg -y -i \"" << segments[i].path << "\" ";
        // Add silent audio source
        cmd << "-f lavfi -i anullsrc=r=48000:cl=stereo ";
        cmd << "-c:v libx264 -preset ultrafast -crf 23 ";
        cmd << "-r " << config_.fps << " ";
        cmd << "-s " << config_.width << "x" << config_.height << " ";
        cmd << "-pix_fmt yuv420p ";
        cmd << "-colorspace bt709 -color_primaries bt709 -color_trc bt709 ";
        cmd << "-map 0:v:0 -map 1:a:0 ";
        
        // Apply trim if needed
        if (segments[i].needsTrim) {
            cmd << "-t " << segments[i].trimmedDuration << " ";
        }
        
        cmd << "-shortest ";
        cmd << "-c:a aac -ar 48000 -ac 2 -b:a 128k ";
        cmd << "-fps_mode cfr ";
        cmd << "-video_track_timescale 90000 ";
        cmd << "-movflags +faststart ";
        cmd << "\"" << normalizedPath.string() << "\" 2>&1";
        
        int result = std::system(cmd.str().c_str());
        if (result != 0 || !fs::exists(normalizedPath)) {
            std::cerr << "  Warning: Failed to normalize segment " << i << ", skipping" << std::endl;
            continue;
        }
        
        normalizedSegments.push_back(normalizedPath.string());
    }
    
    if (normalizedSegments.empty()) {
        throw std::runtime_error("Failed to normalize any video segments");
    }
    
    std::cout << "  Successfully normalized " << normalizedSegments.size() << " segments" << std::endl;
    
    // If only one segment after normalization, return it directly
    if (normalizedSegments.size() == 1) {
        double duration = getVideoDuration(normalizedSegments[0]);
        std::cout << "  Single normalized segment ready, duration: " << duration << " seconds" << std::endl;
        return normalizedSegments[0];
    }
    
    // Create concat demuxer file
    fs::path concatFile = tempDir_ / "concat.txt";
    std::ofstream concat(concatFile);
    if (!concat.is_open()) {
        throw std::runtime_error("Failed to create concat file");
    }
    
    for (const auto& segment : normalizedSegments) {
        concat << "file '" << fs::absolute(segment).string() << "'\n";
    }
    concat.close();
    
    // Output path for stitched video
    fs::path outputPath = tempDir_ / "background_stitched.mp4";
    tempFiles_.push_back(outputPath);
    
    // Build ffmpeg command
    std::ostringstream cmd;
    cmd << "ffmpeg -y -fflags +genpts -f concat -safe 0 -i \"" << concatFile.string() << "\" ";
    cmd << "-c copy ";
    cmd << "-movflags +faststart ";
    cmd << "\"" << outputPath.string() << "\" 2>&1";
    
    std::cout << "  Concatenating normalized segments..." << std::endl;
    
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        throw std::runtime_error("Failed to stitch videos with ffmpeg");
    }
    
    if (!fs::exists(outputPath)) {
        throw std::runtime_error("Stitched video file not created");
    }
    
    double stitchedDuration = getVideoDuration(outputPath.string());
    std::cout << "  Stitched video created, duration: " << stitchedDuration << " seconds" << std::endl;
    
    return outputPath.string();
}

std::string Manager::prepareBackgroundVideo(double totalDurationSeconds) {
    if (!config_.videoSelection.enableDynamicBackgrounds) {
        return config_.assetBgVideo;
    }

    try {
        std::cout << "Selecting dynamic background videos..." << std::endl;
        
        std::vector<VideoSegment> segments = collectVideoSegments(totalDurationSeconds);
        
        if (segments.empty()) {
            std::cerr << "Warning: No video segments collected, using default background" << std::endl;
            return config_.assetBgVideo;
        }
        
        std::string finalVideo = stitchVideos(segments);
        
        double finalDuration = getVideoDuration(finalVideo);
        if (finalDuration > 0 && finalDuration < totalDurationSeconds) {
            std::cout << "  Note: Background duration (" << finalDuration 
                     << "s) < total duration (" << totalDurationSeconds 
                     << "s), will loop automatically" << std::endl;
        }
        
        std::cout << "  Background video ready: " << finalVideo << std::endl;
        return finalVideo;
        
    } catch (const std::exception& e) {
        std::cerr << "Warning: Dynamic background selection failed: " << e.what() 
                 << ", using default background" << std::endl;
        return config_.assetBgVideo;
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