#pragma once

#include <string>
#include "types.h"

namespace LocalizationUtils {
    std::string getLanguageCode(const AppConfig& config);
    std::string getLocalizedSurahName(int surah, const std::string& lang_code);
    std::string getLocalizedReciterName(int reciterId, const std::string& lang_code);
    std::string getLocalizedSurahLabel(const std::string& lang_code);
    std::string getLocalizedNumber(int value, const std::string& lang_code);
    std::string reverseWords(const std::string& text);
}
