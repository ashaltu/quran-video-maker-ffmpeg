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
    void shuffle(std::vector<std::pair<std::string, std::string>>& items);

private:
    std::mt19937 gen;
};

// A single entry in a range's playlist
struct PlaylistEntry {
    std::string theme;
    std::string videoKey;
};

struct SelectionState {
    // Cached playlists per range (built once, then cycled)
    std::map<std::string, std::vector<PlaylistEntry>> rangePlaylists;
    // Current position in each range's playlist
    std::map<std::string, size_t> rangePlaylistIndices;
};

// Represents a verse range with its themes and time allocation
struct VerseRangeSegment {
    int startVerse;
    int endVerse;
    std::vector<std::string> themes;
    double startTimeFraction;  // 0.0 to 1.0 - when this range starts
    double endTimeFraction;    // 0.0 to 1.0 - when this range ends
    std::string rangeKey;      // e.g., "19:10-15" for tracking
};

class Selector {
public:
    explicit Selector(const std::string& metadataPath, unsigned int seed = 99);
    
    // Get verse range segments with time allocations for the requested range
    std::vector<VerseRangeSegment> getVerseRangeSegments(int surah, int from, int to);
    
    // Get the range segment for a specific time position (0.0 to 1.0)
    const VerseRangeSegment* getRangeForTimePosition(
        const std::vector<VerseRangeSegment>& segments,
        double timeFraction);
    
    // Build or get the playlist for a range
    const std::vector<PlaylistEntry>& getOrBuildPlaylist(
        const VerseRangeSegment& range,
        const std::map<std::string, std::vector<std::string>>& themeVideosCache,
        SelectionState& state);
    
    // Get the next video from a range's playlist (cycles automatically)
    PlaylistEntry getNextVideoForRange(
        const std::string& rangeKey,
        SelectionState& state);

private:
    nlohmann::json metadata;
    SeededRandom random;
    
    std::vector<std::string> findRangeForVerse(int surah, int verse);
    std::pair<int, int> findRangeBoundsForVerse(int surah, int verse);
    
    // Build an interleaved playlist from themes and their videos
    std::vector<PlaylistEntry> buildPlaylist(
        const std::vector<std::string>& themes,
        const std::map<std::string, std::vector<std::string>>& themeVideosCache);
};

} // namespace VideoSelector