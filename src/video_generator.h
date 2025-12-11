#pragma once
#include "types.h"
#include "interfaces/IProcessExecutor.h"
#include "verse_segmentation.h"
#include <vector>
#include <memory>

namespace VideoGenerator {
    void generateVideo(const CLIOptions& options, 
                       const AppConfig& config, 
                       const std::vector<VerseData>& verses, 
                       std::shared_ptr<Interfaces::IProcessExecutor> processExecutor,
                       const VerseSegmentation::Manager* segmentManager = nullptr);
    void generateThumbnail(const CLIOptions& options, 
                           const AppConfig& config, 
                           std::shared_ptr<Interfaces::IProcessExecutor> processExecutor);
}