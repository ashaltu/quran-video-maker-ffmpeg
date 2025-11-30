#pragma once
#include "types.h"
#include <vector>

#include <string>

namespace VideoGenerator {
    void generateVideo(const CLIOptions& options, const AppConfig& config, const std::vector<VerseData>& verses, const std::string& mockCommandOutputPath = "");
    void generateThumbnail(const CLIOptions& options, const AppConfig& config, const std::string& mockCommandOutputPath = "");
}
