#pragma once

#include "types.h"
#include <string>

namespace TextLayout {

struct LayoutResult {
    std::string wrappedArabic;
    std::string wrappedTranslation;
    int baseArabicSize = 0;
    int baseTranslationSize = 0;
    double arabicGrowthFactor = 1.0;
    double translationGrowthFactor = 1.0;
    bool growArabic = false;
    int arabicWordCount = 0;
};

class Engine {
public:
    explicit Engine(const AppConfig& config);

    LayoutResult layoutVerse(const VerseData& verse) const;
    LayoutResult layoutSegment(const std::string& arabic, const std::string& translation, double durationSeconds) const;
    double paddingPixels() const { return paddingPixels_; }
    double arabicWrapWidth() const { return arabicWrapWidth_; }
    double translationWrapWidth() const { return translationWrapWidth_; }

private:
    const AppConfig& config_;
    double paddingPixels_;
    double arabicWrapWidth_;
    double translationWrapWidth_;
};

} // namespace TextLayout
