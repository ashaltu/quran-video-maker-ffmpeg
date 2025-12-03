#pragma once
#include "types.h"
#include <string>
#include <filesystem>

namespace BackgroundVideo {

class Manager {
public:
    explicit Manager(const AppConfig& config, const CLIOptions& options);
    
    // Select and prepare background video for the given verse range and total duration
    std::string prepareBackgroundVideo(double totalDurationSeconds);
    
    // Cleanup temporary files
    void cleanup();

private:
    const AppConfig& config_;
    const CLIOptions& options_;
    std::filesystem::path tempDir_;
    std::vector<std::filesystem::path> tempFiles_;
};

} // namespace BackgroundVideo