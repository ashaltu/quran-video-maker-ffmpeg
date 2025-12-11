#include "verse_segmentation.h"
#include "cache_utils.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace VerseSegmentation {

bool Manager::loadLongVersesList(const std::string& path) {
    try {
        fs::path resolvedPath = CacheUtils::resolveDataPath(path);
        
        if (!fs::exists(resolvedPath)) {
            std::cerr << "Warning: Long verses list not found: " << resolvedPath << std::endl;
            return false;
        }
        
        std::ifstream file(resolvedPath);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open long verses list: " << resolvedPath << std::endl;
            return false;
        }
        
        json data = json::parse(file);
        if (!data.is_array()) {
            std::cerr << "Warning: Long verses list must be a JSON array" << std::endl;
            return false;
        }
        
        longVerses_.clear();
        for (const auto& item : data) {
            if (item.is_string()) {
                longVerses_.insert(item.get<std::string>());
            }
        }
        
        std::cout << "  Loaded " << longVerses_.size() << " long verse definitions from " 
                  << resolvedPath.filename() << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading long verses list: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::loadSegmentData(const std::string& path) {
    try {
        fs::path resolvedPath = path;
        
        // Try to resolve relative to data root if not absolute
        if (!resolvedPath.is_absolute()) {
            fs::path dataRelative = CacheUtils::resolveDataPath(path);
            if (fs::exists(dataRelative)) {
                resolvedPath = dataRelative;
            } else if (!fs::exists(resolvedPath)) {
                std::cerr << "Warning: Segment data file not found: " << path << std::endl;
                return false;
            }
        }
        
        if (!fs::exists(resolvedPath)) {
            std::cerr << "Warning: Segment data file not found: " << resolvedPath << std::endl;
            return false;
        }
        
        std::ifstream file(resolvedPath);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open segment data file: " << resolvedPath << std::endl;
            return false;
        }
        
        json data = json::parse(file);
        if (!data.is_object()) {
            std::cerr << "Warning: Segment data must be a JSON object with verse keys" << std::endl;
            return false;
        }
        
        segmentData_.clear();
        int totalSegments = 0;
        
        for (auto& [verseKey, segments] : data.items()) {
            // Skip comment fields
            if (verseKey.find('_') == 0) continue;
            
            if (!segments.is_array()) {
                std::cerr << "Warning: Segments for " << verseKey << " is not an array, skipping" << std::endl;
                continue;
            }
            
            std::vector<Segment> verseSegments;
            for (const auto& seg : segments) {
                if (!seg.is_object()) continue;
                
                Segment segment;
                segment.startSeconds = seg.value("start", 0.0);
                segment.endSeconds = seg.value("end", 0.0);
                segment.arabic = seg.value("arabic", "");
                segment.translation = seg.value("translation", "");
                segment.isLast = seg.value("is_last", true);
                
                // Validate segment has reasonable data
                if (segment.endSeconds > segment.startSeconds && 
                    (!segment.arabic.empty() || !segment.translation.empty())) {
                    verseSegments.push_back(segment);
                    totalSegments++;
                }
            }
            
            if (!verseSegments.empty()) {
                segmentData_[verseKey] = verseSegments;
            }
        }
        
        std::cout << "  Loaded segment data: " << segmentData_.size() << " verses, " 
                  << totalSegments << " total segments from " << resolvedPath.filename() << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading segment data: " << e.what() << std::endl;
        return false;
    }
}

bool Manager::isLongVerse(const std::string& verseKey) const {
    return longVerses_.find(verseKey) != longVerses_.end();
}

bool Manager::hasSegmentData(const std::string& verseKey) const {
    return segmentData_.find(verseKey) != segmentData_.end();
}

std::vector<Segment> Manager::getSegments(const std::string& verseKey) const {
    auto it = segmentData_.find(verseKey);
    if (it != segmentData_.end()) {
        return it->second;
    }
    return {};
}

bool Manager::shouldSegmentVerse(const std::string& verseKey) const {
    if (!enabled_) return false;
    return isLongVerse(verseKey) && hasSegmentData(verseKey);
}

std::unique_ptr<Manager> createManager(bool enabled,
                                        const std::string& longVersesPath,
                                        const std::string& segmentDataPath) {
    auto manager = std::make_unique<Manager>();
    manager->setEnabled(enabled);
    
    if (!enabled) {
        return manager;
    }
    
    std::cout << "Initializing verse segmentation..." << std::endl;
    
    // Load long verses list
    if (!longVersesPath.empty()) {
        manager->loadLongVersesList(longVersesPath);
    }
    
    // Load segment data
    if (!segmentDataPath.empty()) {
        if (!manager->loadSegmentData(segmentDataPath)) {
            std::cerr << "Warning: Failed to load segment data, segmentation will be disabled" << std::endl;
            manager->setEnabled(false);
        }
    } else {
        std::cerr << "Warning: No segment data path provided, segmentation will be disabled" << std::endl;
        manager->setEnabled(false);
    }
    
    if (manager->isEnabled()) {
        std::cout << "  Verse segmentation enabled" << std::endl;
    }
    
    return manager;
}

} // namespace VerseSegmentation