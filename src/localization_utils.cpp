#include "localization_utils.h"
#include "quran_data.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <vector>
#include <cctype>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
json load_json_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return json();
    json data = json::parse(f, nullptr, false);
    if (data.is_discarded()) return json();
    return data;
}
}

namespace LocalizationUtils {

std::string getLanguageCode(const AppConfig& config) {
    return QuranData::getTranslationLanguageCode(config.translationId);
}

std::string getLocalizedSurahName(int surah, const std::string& lang_code) {
    fs::path path = fs::path("data/surah-names") / (lang_code + ".json");
    json data = load_json_file(path);
    std::string key = std::to_string(surah);
    if (data.is_object()) {
        auto it = data.find(key);
        if (it != data.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return QuranData::surahNames.at(surah);
}

std::string getLocalizedReciterName(int reciterId, const std::string& lang_code) {
    fs::path path = fs::path("data/reciter-names") / (lang_code + ".json");
    json data = load_json_file(path);
    std::string key = std::to_string(reciterId);
    if (data.is_object()) {
        auto it = data.find(key);
        if (it != data.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return QuranData::reciterNames.at(reciterId);
}

std::string getLocalizedSurahLabel(const std::string& lang_code) {
    json data = load_json_file("data/misc/surah.json");
    if (data.is_object()) {
        auto it = data.find(lang_code);
        if (it != data.end() && it->is_string()) {
            return it->get<std::string>();
        }
        auto fallback = data.find("en");
        if (fallback != data.end() && fallback->is_string()) {
            return fallback->get<std::string>();
        }
    }
    return "Surah";
}

std::string getLocalizedNumber(int value, const std::string& lang_code) {
    json data = load_json_file("data/misc/numbers.json");
    auto lookup_for_lang = [&](const std::string& code) -> std::string {
        if (!data.is_object()) return "";
        auto lang_it = data.find(code);
        if (lang_it != data.end() && lang_it->is_object()) {
            std::string key = std::to_string(value);
            auto number_it = lang_it->find(key);
            if (number_it != lang_it->end() && number_it->is_string()) {
                return number_it->get<std::string>();
            }
        }
        return "";
    };

    std::string localized = lookup_for_lang(lang_code);
    if (localized.empty()) {
        localized = lookup_for_lang("en");
    }
    if (localized.empty()) {
        localized = std::to_string(value);
    }
    return localized;
}

std::string reverseWords(const std::string& text) {
    std::istringstream iss(text);
    std::vector<std::string> words;
    std::string word;
    
    // Helper to detect if a char is punctuation we want to move
    auto is_punct = [](char c) {
        static const std::string punct = "()[]{}\"\'.,!?-;:<>";
        return punct.find(c) != std::string::npos;
    };

    while (iss >> word) {
        // Split word into: prefix_punct + core + suffix_punct
        // We want to transform it to: suffix_punct + core + prefix_punct
        // Because in RTL, the "Start" (Right) punctuation should be on the Right of the word.
        // When we reverse word order for LTR renderer, the Right of the word becomes the "End" of the string.
        // So Prefix (Start) -> End (Suffix). Suffix (End) -> Start (Prefix).

        if (word.empty()) continue;

        std::string prefix;
        std::string suffix;
        std::string core = word;

        // Extract prefix
        size_t prefix_len = 0;
        while (prefix_len < core.size() && is_punct(core[prefix_len])) {
            prefix_len++;
        }
        if (prefix_len > 0) {
            prefix = core.substr(0, prefix_len);
            core = core.substr(prefix_len);
        }

        // Extract suffix (if any core remains)
        if (!core.empty()) {
            size_t suffix_len = 0;
            while (suffix_len < core.size() && is_punct(core[core.size() - 1 - suffix_len])) {
                suffix_len++;
            }
            if (suffix_len > 0) {
                suffix = core.substr(core.size() - suffix_len);
                core = core.substr(0, core.size() - suffix_len);
            }
        } else {
            // If word was all punctuation, prefix took it all.
            // e.g. "..." -> Prefix "...", Core "", Suffix "".
            // We can just leave it as is, or swap?
            // "..." -> "..." (Symmetric).
            // "(" -> "(".
            // If we have just "(", it is prefix. 
            // Swap -> Suffix "(". Core "". Prefix "".
            // Result "(" (same).
        }
        
        // Reconstruct: Suffix + Core + Prefix
        words.push_back(suffix + core + prefix);
    }
    
    if (words.empty()) return text;
    std::reverse(words.begin(), words.end());
    
    std::string result;
    for (size_t i = 0; i < words.size(); ++i) {
        // Mirror characters for RTL
        std::string& w = words[i];
        for (char& c : w) {
            switch(c) {
                case '(': c = ')'; break;
                case ')': c = '('; break;
                case '[': c = ']'; break;
                case ']': c = '['; break;
                case '{': c = '}'; break;
                case '}': c = '{'; break;
                case '<': c = '>'; break;
                case '>': c = '<'; break;
                default: break;
            }
        }

        result += w;
        if (i < words.size() - 1) {
            result += " ";
        }
    }
    return result;
}

} // namespace LocalizationUtils
