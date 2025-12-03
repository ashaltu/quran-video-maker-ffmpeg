#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <nlohmann/json.hpp>

namespace VideoSelector {

class SeededRandom {
public:
    explicit SeededRandom(unsigned int seed);
    int nextInt(int min, int max);
    
    template<typename T>
    const T& choice(const std::vector<T>& items);

private:
    std::mt19937 gen;
};

struct SelectionState {
    std::map<std::string, std::set<std::string>> usedVideos;  // theme -> set of used video keys
    std::map<std::string, std::vector<std::string>> exhaustedThemes;  // surah:verses -> exhausted themes
};

class Selector {
public:
    explicit Selector(const std::string& metadataPath, unsigned int seed = 99);
    
    // Get themes for a verse range
    std::vector<std::string> getThemesForVerses(int surah, int from, int to);
    
    // Select a theme ensuring no repeats until exhausted
    std::string selectTheme(const std::vector<std::string>& themes, 
                           const std::string& verseRange,
                           SelectionState& state);
    
    // Select a video from theme ensuring no repeats until exhausted
    std::string selectVideoFromTheme(const std::string& theme,
                                    const std::vector<std::string>& availableVideos,
                                    SelectionState& state);

private:
    nlohmann::json metadata;
    SeededRandom random;
    
    std::vector<int> parseVerseRange(const std::string& rangeStr);
    std::vector<std::string> findRangeForVerse(int surah, int verse);
};

} // namespace VideoSelector