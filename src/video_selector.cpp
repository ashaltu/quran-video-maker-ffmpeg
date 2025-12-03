#include "video_selector.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

namespace VideoSelector {

SeededRandom::SeededRandom(unsigned int seed) : gen(seed) {}

int SeededRandom::nextInt(int min, int max) {
    std::uniform_int_distribution<> dis(min, max - 1);
    return dis(gen);
}

template<typename T>
const T& SeededRandom::choice(const std::vector<T>& items) {
    if (items.empty()) {
        throw std::runtime_error("Cannot choose from empty vector");
    }
    return items[nextInt(0, items.size())];
}

Selector::Selector(const std::string& metadataPath, unsigned int seed)
    : random(seed) {
    std::ifstream file(metadataPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open theme metadata: " + metadataPath);
    }
    file >> metadata;
}

std::vector<int> Selector::parseVerseRange(const std::string& rangeStr) {
    std::vector<int> verses;
    std::istringstream iss(rangeStr);
    std::string part;
    
    while (std::getline(iss, part, ',')) {
        size_t dashPos = part.find('-');
        if (dashPos != std::string::npos) {
            int start = std::stoi(part.substr(0, dashPos));
            int end = std::stoi(part.substr(dashPos + 1));
            for (int i = start; i <= end; ++i) {
                verses.push_back(i);
            }
        } else {
            verses.push_back(std::stoi(part));
        }
    }
    
    std::sort(verses.begin(), verses.end());
    verses.erase(std::unique(verses.begin(), verses.end()), verses.end());
    return verses;
}

std::vector<std::string> Selector::findRangeForVerse(int surah, int verse) {
    std::string surahKey = std::to_string(surah);
    if (!metadata.contains(surahKey)) {
        return {};
    }
    
    const auto& surahData = metadata[surahKey];
    for (auto it = surahData.begin(); it != surahData.end(); ++it) {
        const std::string& range = it.key();
        size_t dashPos = range.find('-');
        if (dashPos == std::string::npos) continue;
        
        int start = std::stoi(range.substr(0, dashPos));
        int end = std::stoi(range.substr(dashPos + 1));
        
        if (verse >= start && verse <= end) {
            return it.value().get<std::vector<std::string>>();
        }
    }
    
    return {};
}

std::vector<std::string> Selector::getThemesForVerses(int surah, int from, int to) {
    std::set<std::string> allThemes;
    
    for (int verse = from; verse <= to; ++verse) {
        auto themes = findRangeForVerse(surah, verse);
        allThemes.insert(themes.begin(), themes.end());
    }
    
    return std::vector<std::string>(allThemes.begin(), allThemes.end());
}

std::string Selector::selectTheme(const std::vector<std::string>& themes,
                                 const std::string& verseRange,
                                 SelectionState& state) {
    if (themes.empty()) {
        throw std::runtime_error("No themes available for selection");
    }
    
    // Filter out exhausted themes for this verse range
    std::vector<std::string> available;
    const auto& exhausted = state.exhaustedThemes[verseRange];
    
    for (const auto& theme : themes) {
        if (std::find(exhausted.begin(), exhausted.end(), theme) == exhausted.end()) {
            available.push_back(theme);
        }
    }
    
    // If all themes exhausted, reset
    if (available.empty()) {
        state.exhaustedThemes[verseRange].clear();
        available = themes;
    }
    
    return random.choice(available);
}

std::string Selector::selectVideoFromTheme(const std::string& theme,
                                          const std::vector<std::string>& availableVideos,
                                          SelectionState& state) {
    if (availableVideos.empty()) {
        throw std::runtime_error("No videos available in theme: " + theme);
    }
    
    auto& used = state.usedVideos[theme];
    std::vector<std::string> unused;
    
    for (const auto& video : availableVideos) {
        if (used.find(video) == used.end()) {
            unused.push_back(video);
        }
    }
    
    // If all videos used, reset
    if (unused.empty()) {
        used.clear();
        unused = availableVideos;
    }
    
    std::string selected = random.choice(unused);
    used.insert(selected);
    
    // If we've now used all videos, mark theme as exhausted
    if (used.size() == availableVideos.size()) {
        // Theme is exhausted for this selection cycle
    }
    
    return selected;
}

} // namespace VideoSelector