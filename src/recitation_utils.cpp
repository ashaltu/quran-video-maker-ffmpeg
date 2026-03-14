#include "recitation_utils.h"
#include "cache_utils.h"
#include <algorithm>

namespace RecitationUtils {

void normalizeGaplessTimings(std::vector<VerseData>& verses) {
    if (verses.empty()) return;
    int lastEnd = verses.front().timestampFromMs;
    for (auto& verse : verses) {
        if (verse.timestampFromMs < lastEnd) {
            verse.timestampFromMs = lastEnd;
        }
        if (verse.timestampToMs <= verse.timestampFromMs) {
            verse.timestampToMs = verse.timestampFromMs + 750;
        }
        verse.durationInSeconds = (verse.timestampToMs - verse.timestampFromMs) / 1000.0;
        lastEnd = verse.timestampToMs;
    }
}

VerseData buildBismillahFromTiming(const TimingEntry& timing,
                                   const AppConfig& config,
                                   const std::string& localAudioPath, 
								   bool skipStartBismillah) {

	if (skipStartBismillah) {  
        // Return empty verse data when skipping Bismillah  
        VerseData empty;  
        return empty;  
    }  
	
    VerseData verse;
    verse.verseKey = "1:1";
    verse.text.clear();
    verse.translation = CacheUtils::getTranslationText(config.translationId, "1:1");
    verse.audioUrl.clear();
    verse.localAudioPath = localAudioPath;
    verse.timestampFromMs = timing.startMs;
    verse.timestampToMs = timing.endMs;
    verse.durationInSeconds = std::max(0.001, (timing.endMs - timing.startMs) / 1000.0);
    verse.absoluteTimestampFromMs = verse.timestampFromMs;
    verse.absoluteTimestampToMs = verse.timestampToMs;
    verse.fromCustomAudio = true;
    verse.sourceAudioPath = localAudioPath;
    return verse;
}

} // namespace RecitationUtils
