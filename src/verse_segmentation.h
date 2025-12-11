#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

namespace VerseSegmentation {

struct Segment {
    double startSeconds;      // Absolute start time in the audio file
    double endSeconds;        // Absolute end time in the audio file
    std::string arabic;       // Arabic text for this segment
    std::string translation;  // Translation text for this segment
    bool isLast;              // Whether this is the last segment of the verse
    
    double duration() const { return endSeconds - startSeconds; }
};

class Manager {
public:
    Manager() = default;
    
    // Load the list of verses considered "long" (e.g., metadata/long-verses.json)
    bool loadLongVersesList(const std::string& path);
    
    // Load reciter-specific segment timing data
    bool loadSegmentData(const std::string& path);
    
    // Check if a verse is in the long verses list
    bool isLongVerse(const std::string& verseKey) const;
    
    // Check if segment data is available for a verse
    bool hasSegmentData(const std::string& verseKey) const;
    
    // Get segments for a verse (returns empty vector if not available)
    std::vector<Segment> getSegments(const std::string& verseKey) const;
    
    // Check if a verse should be segmented (is long AND has segment data)
    bool shouldSegmentVerse(const std::string& verseKey) const;
    
    // Getters/setters for enabled state
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    // Get count of loaded data for logging
    size_t longVersesCount() const { return longVerses_.size(); }
    size_t segmentDataCount() const { return segmentData_.size(); }

private:
    bool enabled_ = false;
    std::set<std::string> longVerses_;
    std::map<std::string, std::vector<Segment>> segmentData_;
};

// Factory function to create and configure a manager
std::unique_ptr<Manager> createManager(bool enabled,
                                        const std::string& longVersesPath,
                                        const std::string& segmentDataPath);

} // namespace VerseSegmentation