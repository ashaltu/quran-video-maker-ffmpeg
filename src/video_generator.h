#pragma once
#include "types.h"
#include "interfaces/IProcessExecutor.h"
#include <vector>
#include <memory>

namespace VideoGenerator {
    void generateVideo(const CLIOptions& options, const AppConfig& config, const std::vector<VerseData>& verses, std::shared_ptr<Interfaces::IProcessExecutor> processExecutor);
    void generateThumbnail(const CLIOptions& options, const AppConfig& config, std::shared_ptr<Interfaces::IProcessExecutor> processExecutor);
}