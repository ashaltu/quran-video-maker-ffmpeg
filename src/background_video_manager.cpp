#include "background_video_manager.h"
#include "video_selector.h"
#include "r2_client.h"
#include <iostream>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
}

namespace fs = std::filesystem;

namespace {

double getVideoDuration(const std::string& path) {
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

} // namespace

namespace BackgroundVideo {

Manager::Manager(const AppConfig& config, const CLIOptions& options)
    : config_(config), options_(options) {
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    tempDir_ = fs::temp_directory_path() / ("qvm_bg_" + std::to_string(timestamp));
    fs::create_directories(tempDir_);
}

std::string Manager::prepareBackgroundVideo(double totalDurationSeconds) {
    // If dynamic backgrounds disabled, use default
    if (!config_.videoSelection.enableDynamicBackgrounds) {
        return config_.assetBgVideo;
    }

    try {
        std::cout << "Selecting dynamic background video..." << std::endl;
        
        // Initialize selector with seed
        VideoSelector::Selector selector(
            config_.videoSelection.themeMetadataPath,
            config_.videoSelection.seed
        );
        
        // Get themes for the verse range
        auto themes = selector.getThemesForVerses(
            options_.surah, 
            options_.from, 
            options_.to
        );
        
        if (themes.empty()) {
            std::cerr << "Warning: No themes found for Surah " << options_.surah 
                     << ", verses " << options_.from << "-" << options_.to 
                     << ", using default background" << std::endl;
            return config_.assetBgVideo;
        }
        
        std::cout << "  Available themes: ";
        for (size_t i = 0; i < themes.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << themes[i];
        }
        std::cout << std::endl;
        
        // Select theme
        VideoSelector::SelectionState state;
        std::string verseRange = std::to_string(options_.surah) + ":" + 
                                std::to_string(options_.from) + "-" + 
                                std::to_string(options_.to);
        
        std::string selectedTheme = selector.selectTheme(themes, verseRange, state);
        std::cout << "  Selected theme: " << selectedTheme << std::endl;
        
        // Initialize R2 client
        R2::R2Config r2Config{
            config_.videoSelection.r2Endpoint,
            config_.videoSelection.r2AccessKey,
            config_.videoSelection.r2SecretKey,
            config_.videoSelection.r2Bucket,
            config_.videoSelection.usePublicBucket
        };
        
        R2::Client r2Client(r2Config);
        auto availableVideos = r2Client.listVideosInTheme(selectedTheme);
        
        if (availableVideos.empty()) {
            std::cerr << "Warning: No videos found for theme '" << selectedTheme 
                     << "', using default background" << std::endl;
            return config_.assetBgVideo;
        }
        
        std::cout << "  Found " << availableVideos.size() << " video(s) in theme" << std::endl;
        
        // Select video
        std::string selectedVideo = selector.selectVideoFromTheme(
            selectedTheme, 
            availableVideos, 
            state
        );
        std::cout << "  Selected video: " << selectedVideo << std::endl;
        
        // Download video
        fs::path localPath = tempDir_ / fs::path(selectedVideo).filename();
        std::string downloadedPath = r2Client.downloadVideo(selectedVideo, localPath);
        tempFiles_.push_back(localPath);
        
        // Verify video duration is suitable
        double videoDuration = getVideoDuration(downloadedPath);
        if (videoDuration > 0 && videoDuration < totalDurationSeconds) {
            std::cout << "  Note: Background video duration (" << videoDuration 
                     << "s) is shorter than total duration (" << totalDurationSeconds 
                     << "s), will loop automatically" << std::endl;
        }
        
        std::cout << "  Background video ready: " << downloadedPath << std::endl;
        return downloadedPath;
        
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