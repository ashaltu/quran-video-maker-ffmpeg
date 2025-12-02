#include "metadata_generator.h"
#include "quran_data.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

std::string getFullTranslationName(const std::string& filename) {
    std::string name = filename.substr(0, filename.find("-clean.json"));
    std::replace(name.begin(), name.end(), '-', ' ');
    name[0] = toupper(name[0]);
    for (size_t i = 1; i < name.length(); ++i) {
        if (name[i - 1] == ' ') {
            name[i] = toupper(name[i]);
        }
    }
    return name;
}

} // namespace

namespace MetadataGenerator {

void generateBackendMetadata() {
    json metadata;

    // Reciters
    json reciters = json::array();
    for (const auto& pair : QuranData::reciterNames) {
        reciters.push_back({
            {"id", pair.first},
            {"name", pair.second}
        });
    }
    metadata["reciters"] = reciters;

    // Translations
    json translations = json::array();
    for (const auto& entry : fs::directory_iterator("data/translations")) {
        if (entry.is_directory()) {
            for (const auto& file : fs::directory_iterator(entry.path())) {
                if (file.is_regular_file() && file.path().extension() == ".json") {
                    for(const auto& translationFile : QuranData::translationFiles) {
                        if (translationFile.second == file.path().string()) {
                            translations.push_back({
                                {"id", translationFile.first},
                                {"name", getFullTranslationName(file.path().filename().string())}
                            });
                        }
                    }
                }
            }
        }
    }
    metadata["translations"] = translations;

    // Surahs
    json surahs = json::object();
    std::ifstream arSurahNamesFile("data/surah-names/ar.json");
    json arSurahNamesData;
    if (arSurahNamesFile.is_open()) {
        arSurahNamesFile >> arSurahNamesData;
    }

    std::ifstream quranFile("data/quran/qpc-hafs-word-by-word.json");
    if (quranFile.is_open()) {
        json quranData;
        quranFile >> quranData;

        std::map<int, int> verseCounts;

        for (auto it = quranData.begin(); it != quranData.end(); ++it) {
            const auto& verse = it.value();
            int surahNum = std::stoi(verse["surah"].get<std::string>());
            int ayahNum = std::stoi(verse["ayah"].get<std::string>());

            if (verseCounts.find(surahNum) == verseCounts.end()) {
                verseCounts[surahNum] = 0;
            }
            if (ayahNum > verseCounts[surahNum]) {
                verseCounts[surahNum] = ayahNum;
            }
        }

        for (int i = 1; i <= 114; ++i) {
            surahs[std::to_string(i)] = {
                {"en_name", QuranData::surahNames.at(i)},
                {"ar_name", arSurahNamesData.count(std::to_string(i)) ? arSurahNamesData[std::to_string(i)] : ""},
                {"verse_count", verseCounts.count(i) ? verseCounts[i] : 0}
            };
        }
    }
     metadata["surahs"] = surahs;

    // Misc
    json misc = json::object();
    std::ifstream surahFile("data/misc/surah.json");
    if (surahFile.is_open()) {
        json surahData;
        surahFile >> surahData;
        misc["surah"] = surahData;
    }
    std::ifstream numbersFile("data/misc/numbers.json");
    if (numbersFile.is_open()) {
        json numbersData;
        numbersFile >> numbersData;
        misc["numbers"] = numbersData;
    }
    metadata["misc"] = misc;


    std::cout << metadata.dump() << std::endl;
}

} // namespace MetadataGenerator
