#pragma once

#include <vector>
#include <string>
#include "types.h"
#include "timing_parser.h"

namespace RecitationUtils {
    void normalizeGaplessTimings(std::vector<VerseData>& verses);
    VerseData buildBismillahFromTiming(const TimingEntry& timing,
                                       const AppConfig& config,
                                       const std::string& localAudioPath,
									   bool skipStartBismillah);
}
