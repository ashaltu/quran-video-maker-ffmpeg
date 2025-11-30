#pragma once
#include "interfaces/IApiClient.h"

class LiveApiClient : public Interfaces::IApiClient {
public:
    std::vector<VerseData> fetchQuranData(const CLIOptions& options, const AppConfig& config) override;
};
