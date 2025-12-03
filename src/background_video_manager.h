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
    double trimmedDuration;  // Duration after trimming (if trimmed)
    bool isLocal;
    bool needsTrim;
};

class Manager {
public:
    explicit Manager(const AppConfig& config, const CLIOptions& options);
    
    // Select and prepare background video(s) for the given verse range and total duration
    std::string prepareBackgroundVideo(double totalDurationSeconds);
    
    // Cleanup temporary files
    void cleanup();

private:
    const AppConfig& config_;
    const CLIOptions& options_;
    std::filesystem::path tempDir_;
    std::vector<std::filesystem::path> tempFiles_;
    VideoSelector::SelectionState selectionState_;
    
    // Download and collect video segments until we meet the duration
    std::vector<VideoSegment> collectVideoSegments(double targetDuration);
    
    // Stitch multiple videos together using ffmpeg
    std::string stitchVideos(const std::vector<VideoSegment>& segments);
    
    // Get video duration using libav
    double getVideoDuration(const std::string& path);
};

} // namespace BackgroundVideo