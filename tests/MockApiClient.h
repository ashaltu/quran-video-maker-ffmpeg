#pragma once
#include "interfaces/IApiClient.h"
#include <nlohmann/json.hpp>
#include <fstream>

class MockApiClient : public Interfaces::IApiClient {
public:
    MockApiClient(const std::string& mockDataPath) : mockDataPath(mockDataPath) {}

    std::vector<VerseData> fetchQuranData(const CLIOptions& options, const AppConfig& config) override {
        std::ifstream f(mockDataPath);
        nlohmann::json data = nlohmann::json::parse(f);
        std::vector<VerseData> verses;
        for (const auto& verse_json : data["verses"]) {
            VerseData verse;
            verse.verseKey = verse_json["verse_key"];
            verse.text = verse_json["text_uthmani"];
            verses.push_back(verse);
        }
        return verses;
    }

private:
    std::string mockDataPath;
};
