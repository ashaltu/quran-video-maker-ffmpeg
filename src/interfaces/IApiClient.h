#pragma once
#include "types.h"
#include <vector>
#include <string>

namespace Interfaces {
    class IApiClient {
    public:
        virtual ~IApiClient() = default;
        virtual std::vector<VerseData> fetchQuranData(const CLIOptions& options, const AppConfig& config) = 0;
    };
}
