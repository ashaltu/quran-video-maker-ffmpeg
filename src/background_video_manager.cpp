#include "background_video_manager.h"
#include "r2_client.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>

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
    
    // Get all available themes for the verse range
    auto availableThemes = selector.getThemesForVerses(
        options_.surah, 
        options_.from, 
        options_.to
    );
    
    if (availableThemes.empty()) {
        throw std::runtime_error("No themes available for the specified verse range");
    }
    
    std::cout << "  Available themes: ";
    for (size_t i = 0; i < availableThemes.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << availableThemes[i];
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
    
    // Keep track of available videos per theme
    std::map<std::string, std::vector<std::string>> themeVideosCache;
    
    // Pre-fetch all available videos for all themes
    for (const auto& theme : availableThemes) {
        try {
            themeVideosCache[theme] = r2Client.listVideosInTheme(theme);
            if (themeVideosCache[theme].empty()) {
                std::cout << "  Warning: No videos found for theme '" << theme << "'" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  Error listing videos for theme '" << theme << "': " << e.what() << std::endl;
        }
    }
    
    // Remove themes with no videos
    auto it = availableThemes.begin();
    while (it != availableThemes.end()) {
        if (themeVideosCache[*it].empty()) {
            it = availableThemes.erase(it);
        } else {
            ++it;
        }
    }
    
    if (availableThemes.empty()) {
        throw std::runtime_error("No themes with available videos found");
    }
    
    // Collect segments until we meet or exceed the target duration
    int segmentCount = 0;
    std::string verseRange = std::to_string(options_.surah) + ":" + 
                             std::to_string(options_.from) + "-" + 
                             std::to_string(options_.to);
    
    while (totalDuration < targetDuration) {
        segmentCount++;
        
        // Select a theme (ensuring no repeats until exhausted)
        std::string selectedTheme;
        try {
            selectedTheme = selector.selectTheme(
                availableThemes, 
                verseRange, 
                selectionState_
            );
        } catch (const std::exception& e) {
            std::cerr << "  Error selecting theme: " << e.what() << std::endl;
            break;
        }
        
        const auto& availableVideos = themeVideosCache[selectedTheme];
        if (availableVideos.empty()) {
            std::cerr << "  Theme '" << selectedTheme << "' has no videos, skipping" << std::endl;
            selectionState_.exhaustedThemes[verseRange].push_back(selectedTheme);
            continue;
        }
        
        // Select a video from this theme (ensuring no repeats until exhausted)
        std::string selectedVideo;
        try {
            selectedVideo = selector.selectVideoFromTheme(
                selectedTheme,
                availableVideos,
                selectionState_
            );
        } catch (const std::exception& e) {
            std::cerr << "  Error selecting video: " << e.what() << std::endl;
            continue;
        }
        
        std::cout << "  Segment " << segmentCount 
                  << " - theme: " << selectedTheme 
                  << ", video: " << fs::path(selectedVideo).filename().string();
        
        // Download the video
        fs::path localPath = tempDir_ / (std::to_string(segmentCount) + "_" + fs::path(selectedVideo).filename().string());
        std::string downloadedPath;
        try {
            downloadedPath = r2Client.downloadVideo(selectedVideo, localPath);
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
        
        std::cout << ", duration: " << videoDuration << "s" << std::endl;
        
        // Add to segments
        VideoSegment segment;
        segment.path = downloadedPath;
        segment.theme = selectedTheme;
        segment.duration = videoDuration;
        segment.isLocal = true;
        segments.push_back(segment);
        
        totalDuration += videoDuration;
        
        // Check if we've exhausted all videos across all themes
        bool allVideosUsed = true;
        for (const auto& theme : availableThemes) {
            const auto& videos = themeVideosCache[theme];
            if (selectionState_.usedVideos[theme].size() < videos.size()) {
                allVideosUsed = false;
                break;
            }
        }
        
        // If all videos have been used once and we still need more duration, reset
        if (allVideosUsed && totalDuration < targetDuration) {
            std::cout << "  All unique videos exhausted, resetting selection state..." << std::endl;
            selectionState_.usedVideos.clear();
            selectionState_.exhaustedThemes.clear();
        }
        
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
    
    // If only one segment, still normalize it for consistent color space
    if (segments.size() == 1) {
        std::cout << "  Single segment, normalizing for consistency..." << std::endl;
    } else {
        std::cout << "  Stitching " << segments.size() << " video segments..." << std::endl;
    }
    
    std::cout << "  Re-encoding segments to ensure compatibility..." << std::endl;
    
    // First pass: re-encode all segments to ensure they have compatible parameters
    std::vector<std::string> normalizedSegments;
    for (size_t i = 0; i < segments.size(); ++i) {
        fs::path normalizedPath = tempDir_ / ("normalized_" + std::to_string(i) + ".mp4");
        tempFiles_.push_back(normalizedPath);
        
        std::ostringstream cmd;
        cmd << "ffmpeg -y -i \"" << segments[i].path << "\" ";
        // Add silent audio source - will be limited to video duration by -shortest
        cmd << "-f lavfi -i anullsrc=r=48000:cl=stereo ";
        cmd << "-c:v libx264 -preset ultrafast -crf 23 ";
        cmd << "-r " << config_.fps << " ";  // Force consistent frame rate
        cmd << "-s " << config_.width << "x" << config_.height << " ";  // Force consistent resolution
        cmd << "-pix_fmt yuv420p ";
        // Force consistent color metadata to prevent filter graph reconfiguration
        cmd << "-colorspace bt709 -color_primaries bt709 -color_trc bt709 ";
        // Map video from source (input 0), audio from silent source (input 1)
        // This ensures all segments have audio and discards any original audio
        cmd << "-map 0:v:0 -map 1:a:0 -shortest ";
        cmd << "-c:a aac -ar 48000 -ac 2 -b:a 128k ";
        cmd << "-fps_mode cfr ";  // Use fps_mode instead of deprecated vsync
        cmd << "-video_track_timescale 90000 ";  // Consistent timescale
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
    
    // Build ffmpeg command - use copy since everything is normalized identically
    // Add fflags +genpts to regenerate timestamps and avoid DTS issues
    std::ostringstream cmd;
    cmd << "ffmpeg -y -fflags +genpts -f concat -safe 0 -i \"" << concatFile.string() << "\" ";
    cmd << "-c copy ";
    cmd << "-movflags +faststart ";
    cmd << "\"" << outputPath.string() << "\" 2>&1";
    
    std::cout << "  Concatenating normalized segments..." << std::endl;
    
    // Execute ffmpeg
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        throw std::runtime_error("Failed to stitch videos with ffmpeg");
    }
    
    // Verify the output exists
    if (!fs::exists(outputPath)) {
        throw std::runtime_error("Stitched video file not created");
    }
    
    double stitchedDuration = getVideoDuration(outputPath.string());
    std::cout << "  Stitched video created, duration: " << stitchedDuration << " seconds" << std::endl;
    
    return outputPath.string();
}

std::string Manager::prepareBackgroundVideo(double totalDurationSeconds) {
    // If dynamic backgrounds disabled, use default
    if (!config_.videoSelection.enableDynamicBackgrounds) {
        return config_.assetBgVideo;
    }

    try {
        std::cout << "Selecting dynamic background videos..." << std::endl;
        
        // Collect video segments to meet the target duration
        std::vector<VideoSegment> segments = collectVideoSegments(totalDurationSeconds);
        
        if (segments.empty()) {
            std::cerr << "Warning: No video segments collected, using default background" << std::endl;
            return config_.assetBgVideo;
        }
        
        // Stitch videos together if needed
        std::string finalVideo = stitchVideos(segments);
        
        // Check if we need to note about looping
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