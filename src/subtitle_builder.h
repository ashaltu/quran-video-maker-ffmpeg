#pragma once

#include <string>
#include <vector>
#include "types.h"

namespace SubtitleBuilder {
    std::string applyLatinFontFallback(const std::string& text,
                                       const std::string& fallbackFont,
                                       const std::string& primaryFont);

    // Reverse word order for RTL languages (e.g., Urdu)
    std::string applyRTLIfNeeded(const std::string& text, bool isRTL);

    std::string buildAssFile(const AppConfig& config,
                             const CLIOptions& options,
                             const std::vector<VerseData>& verses,
                             double introDuration,
                             double pauseAfterIntroDuration);
}
