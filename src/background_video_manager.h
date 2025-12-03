#pragma once
#include "types.h"
#include "video_selector.h"
#include <string>
#include <vector>
#include <filesystem>

namespace BackgroundVideo {

struct VideoSegment {
    std::string path;
    std::string theme;
    double duration;
    double trimmedDuration;
    bool isLocal;
    bool needsTrim;
};

class Manager {
public:
    explicit Manager(const AppConfig& config, const CLIOptions& options);
    
    // Build filter complex for dynamic backgrounds (no pre-stitching)
    std::string buildFilterComplex(double totalDurationSeconds, 
                                   std::vector<std::string>& outputInputFiles);
    
    // Cleanup temporary files
    void cleanup();

private:
    const AppConfig& config_;
    const CLIOptions& options_;
    std::filesystem::path tempDir_;
    std::filesystem::path cacheDir_;
    std::vector<std::filesystem::path> tempFiles_;
    VideoSelector::SelectionState selectionState_;
    
    // Get video duration using libav
    double getVideoDuration(const std::string& path);
    
    // Cache management for R2 videos
    std::string getCachedVideoPath(const std::string& remoteKey);
    bool isVideoCached(const std::string& remoteKey);
    void cacheVideo(const std::string& remoteKey, const std::string& localPath);
    
    // Local directory support
    std::vector<std::string> listLocalVideos(const std::string& theme);
};

} // namespace BackgroundVideo