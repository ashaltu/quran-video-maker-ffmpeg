#pragma once
#include "types.h"
#include <vector>
#include <string>

namespace API {
    std::vector<VerseData> fetchQuranData(const CLIOptions& options, const AppConfig& config, const std::string& mockDataPath = "");
}
