#pragma once
#include <string>
#include <vector>

enum class RecitationMode {
    GAPPED,   // ayah-by-ayah
    GAPLESS   // surah-by-surah
};

struct FontConfig {
    std::string family;
    std::string file;
    int size;
    std::string color;
};

struct VideoSelectionConfig {
    std::string r2Endpoint;
    std::string r2AccessKey;
    std::string r2SecretKey;
    std::string r2Bucket = "quran-background-videos";
    std::string themeMetadataPath = "metadata/surah-themes.json";
    unsigned int seed = 99;
    bool enableDynamicBackgrounds = false;
    bool usePublicBucket = true;  // Default to public access
    bool useLocalDirectory = false;  // Use local directory instead of R2
    std::string localVideoDirectory = "";  // Path to local video directory
};

struct AppConfig {
    // Video dimensions
    int width;
    int height;
    int fps;
    
    // Content selection
    int reciterId;
    int translationId;
    bool translationIsRtl;
    RecitationMode recitationMode;  // gapped vs gapless
    
    // Font configuration
    FontConfig arabicFont;
    FontConfig translationFont;
	FontConfig surahHeaderFont;
    std::string translationFallbackFontFamily;
    
    // Visual styling
    std::string overlayColor;
    std::string assetFolderPath;
    std::string assetBgVideo;
    
    // Data paths
    std::string quranWordByWordPath;
    
    // Timing parameters
    double introDuration;           // seconds
    double pauseAfterIntroDuration; // seconds
    int introFadeOutMs;             // milliseconds
    
    // Text animation parameters
    bool enableTextGrowth;
    int textGrowthThreshold;        // word count threshold for growth
    double maxGrowthFactor;         // maximum text growth multiplier
    double growthRateFactor;        // growth rate per second
    
    // Fade parameters
    double fadeDurationFactor;      // fraction of verse duration
    double minFadeDuration;         // minimum fade time in seconds
    double maxFadeDuration;         // maximum fade time in seconds
    
    // Text wrapping parameters
    int textWrapThreshold;          // word count threshold for wrapping
    double arabicMaxWidthFraction;  // max fraction of screen width for Arabic
    double translationMaxWidthFraction; // max fraction of screen width for translation
    double textHorizontalPadding;   // fraction of width reserved as padding on each side
    double textVerticalPadding;     // fraction of height reserved for margins
    
    // Layout parameters
    double verticalShift;           // pixels to shift text vertically
    
    // Thumbnail parameters
    std::vector<std::string> thumbnailColors;
    int thumbnailNumberPadding;     // pixels from edge

    // Quality / encoder parameters
    std::string qualityProfile;
    int crf;
    std::string pixelFormat;
    std::string videoBitrate;
    std::string videoMaxRate;
    std::string videoBufSize;

    // R2 dynamic video selection configuration
    VideoSelectionConfig videoSelection;
};

// Word segment timing information for gapless mode
struct WordSegment {
    int wordIndex;
    int startMs;
    int endMs;
};

struct VerseData {
    std::string verseKey;
    std::string text;
    std::string translation;
    std::string audioUrl;
    double durationInSeconds;
    std::string localAudioPath;
    
    // For gapless mode - timing information
    int timestampFromMs;  // when this verse starts in the surah audio
    int timestampToMs;    // when this verse ends in the surah audio
    std::vector<WordSegment> wordSegments;  // word-level timing

    // Original metadata to support custom audio adjustments
    int absoluteTimestampFromMs = 0;
    int absoluteTimestampToMs = 0;
    bool fromCustomAudio = false;
    std::string sourceAudioPath;
};

struct CLIOptions {
    int surah;
    int from;
    int to;
    std::string configPath = "./config.json";
    bool configPathProvided = false;
    int reciterId = -1;
    int translationId = -1;
    std::string output = "";
    int width = -1;
    int height = -1;
    int fps = -1;
    int arabicFontSize = -1;
    int translationFontSize = -1;
    bool noCache = false;
    bool clearCache = false;
    std::string preset = "fast";
    std::string encoder = "software";
    std::string recitationMode = "";  // "gapped" or "gapless"
    bool presetProvided = false;
    bool emitProgress = false;
	bool showSurahHeader = false;
	int surahHeaderFontSize = 50;  
	int surahHeaderMarginTop = 300;
	bool skipStartBismillah = false;
    
    // Custom recitation support (gapless only)
    std::string customAudioPath = "";     // Path or URL to audio file
    std::string customTimingFile = "";    // Path to VTT or SRT timing file
    
    // Animation control
    bool enableTextGrowth = true;  // Can be overridden via CLI
    double textPaddingOverride = -1.0;

    // Quality overrides
    std::string qualityProfile = "";
    int customCRF = -1;
    std::string pixelFormatOverride = "";
    std::string videoBitrateOverride = "";
    std::string videoMaxRateOverride = "";
    std::string videoBufSizeOverride = "";

    // R2 dynamic video selection configuration
    VideoSelectionConfig videoSelection;

    // Verse segmentation options (for breaking up long verses into timed segments)
    bool segmentLongVerses = false;
    std::string segmentDataPath = "";           // Path to reciter-specific segment timing JSON
    std::string longVersesPath = "metadata/long-verses.json";  // Path to list of long verses
};
