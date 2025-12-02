#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace MetadataWriter {

void writeMetadata(const CLIOptions& options,
                   const AppConfig& config,
                   const std::vector<std::string>& rawArgs);

void generateBackendMetadata(const std::string& outputPath);

} // namespace MetadataWriter
