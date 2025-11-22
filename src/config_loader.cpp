#include "config_loader.h"
#include "quran_data.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <map>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

fs::path getExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path);
#elif __APPLE__
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return fs::canonical(fs::path(path));
    }
    return fs::path();
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        return fs::canonical(fs::path(std::string(path, count)));
    }
    return fs::path();
#endif
}

struct QualityProfileSettings {
    std::string preset;
    int crf;
    std::string pixelFormat;
    std::string videoBitrate;
    std::string videoMaxRate;
    std::string videoBufSize;
};

std::map<std::string, QualityProfileSettings> defaultQualityProfiles() {
    return {
        {"speed",    {"ultrafast", 25, "yuv420p",    "",       "",        ""}},
        {"balanced", {"fast",      21, "yuv420p",    "4500k",  "",        ""}},
        {"max",      {"slow",      18, "yuv420p10le","8000k",  "10000k",  "12000k"}}
    };
}

std::string toLowerCopy(const std::string& value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

std::map<std::string, QualityProfileSettings> loadQualityProfiles(const json& data) {
    auto profiles = defaultQualityProfiles();
    if (!data.contains("qualityProfiles") || !data["qualityProfiles"].is_object()) {
        return profiles;
    }
    for (const auto& [key, value] : data["qualityProfiles"].items()) {
        if (!value.is_object()) continue;
        QualityProfileSettings settings;
        auto normalized = toLowerCopy(key);
        auto existing = profiles.find(normalized);
        if (existing != profiles.end()) settings = existing->second;
        if (value.contains("preset")) settings.preset = value["preset"].get<std::string>();
        if (value.contains("crf")) settings.crf = value["crf"].get<int>();
        if (value.contains("pixelFormat")) settings.pixelFormat = value["pixelFormat"].get<std::string>();
        if (value.contains("videoBitrate")) settings.videoBitrate = value["videoBitrate"].get<std::string>();
        if (value.contains("videoMaxRate")) settings.videoMaxRate = value["videoMaxRate"].get<std::string>();
        if (value.contains("videoBufSize")) settings.videoBufSize = value["videoBufSize"].get<std::string>();
        profiles[normalized] = settings;
    }
    return profiles;
}

void applyQualityProfile(AppConfig& cfg,
                         CLIOptions& options,
                         const std::map<std::string, QualityProfileSettings>& profiles) {
    auto profileName = toLowerCopy(cfg.qualityProfile);
    auto it = profiles.find(profileName);
    if (it == profiles.end()) {
        if (!cfg.qualityProfile.empty() && cfg.qualityProfile != "balanced") {
            std::cerr << "Warning: Unknown quality profile '" << cfg.qualityProfile << "'. Using custom values." << std::endl;
        }
        return;
    }
    const QualityProfileSettings& defaults = it->second;
    if (!options.presetProvided && !defaults.preset.empty()) {
        options.preset = defaults.preset;
    }
    if (cfg.crf <= 0 && defaults.crf > 0) {
        cfg.crf = defaults.crf;
    }
    if (cfg.pixelFormat.empty() && !defaults.pixelFormat.empty()) {
        cfg.pixelFormat = defaults.pixelFormat;
    }
    if (cfg.videoBitrate.empty() && !defaults.videoBitrate.empty()) {
        cfg.videoBitrate = defaults.videoBitrate;
    }
    if (cfg.videoMaxRate.empty() && !defaults.videoMaxRate.empty()) {
        cfg.videoMaxRate = defaults.videoMaxRate;
    }
    if (cfg.videoBufSize.empty() && !defaults.videoBufSize.empty()) {
        cfg.videoBufSize = defaults.videoBufSize;
    }
}
}

AppConfig loadConfig(const std::string& path, CLIOptions& options) {
    fs::path configPath = path;
    
    // Auto-discovery logic
    if (!options.configPathProvided && !fs::exists(configPath)) {
        fs::path exeDir = getExecutablePath().parent_path();
        
        // 1. Try executable directory
        fs::path localConfig = exeDir / "config.json";
        if (fs::exists(localConfig)) {
            configPath = localConfig;
        } else {
            // 2. Try share directory (Homebrew/Linux installation layout)
            // Expected: <prefix>/bin/quran-video-maker -> <prefix>/share/quran-video-maker/config.json
            fs::path shareConfig = exeDir.parent_path() / "share" / "quran-video-maker" / "config.json";
            if (fs::exists(shareConfig)) {
                configPath = shareConfig;
            }
        }
    }

    std::ifstream f(configPath);
    if (!f.is_open()) throw std::runtime_error("Could not open config file: " + configPath.string());
    
    // Resolve assets relative to config file location
    fs::path configDir = fs::absolute(configPath).parent_path();
    auto resolvePath = [&](std::string p) {
        if (p.empty()) return p;
        fs::path fp = p;
        if (fp.is_absolute()) return fp.string();
        return (configDir / fp).string();
    };

    json data = json::parse(f);
    AppConfig cfg;

    // Video dimensions
    cfg.width = data.value("width", 1280);
    cfg.height = data.value("height", 720);
    cfg.fps = data.value("fps", 30);
    
    // Content selection
    cfg.reciterId = data.value("reciterId", 7);
    cfg.translationId = data.value("translationId", 1);
    cfg.translationIsRtl = QuranData::isTranslationRtl(cfg.translationId);
    
    // Recitation mode
    std::string modeStr = data.value("recitationMode", "gapped");
    if (modeStr == "gapless") {
        cfg.recitationMode = RecitationMode::GAPLESS;
    } else {
        cfg.recitationMode = RecitationMode::GAPPED;
    }
    
    // Visual styling
    cfg.overlayColor = data.value("overlayColor", "0x000000@0.5");
    cfg.assetFolderPath = resolvePath(data.value("assetFolderPath", "assets"));

    auto resolveAssetPath = [&](const std::string& assetPath) -> std::string {
        if (assetPath.empty()) return assetPath;
        fs::path fp = assetPath;
        if (fp.is_absolute()) return fp.string();

        fs::path configRelative = configDir / fp;
        if (fs::exists(configRelative)) return configRelative.string();

        fs::path assetRelative = fs::path(cfg.assetFolderPath) / fp;
        if (fs::exists(assetRelative)) return assetRelative.string();

        // Fall back to asset folder path even if it doesn't exist yet (validate later).
        return assetRelative.string();
    };

    std::string bgVideoSetting = data.value("assetBgVideo", QuranData::defaultBackgroundVideo);
    cfg.assetBgVideo = resolveAssetPath(bgVideoSetting);

    // Font configuration
    cfg.arabicFont.family = data["arabicFont"].value("family", "KFGQPC HAFS Uthmanic Script");
    // Logic for font paths: config might just say "fonts/File.ttf".
    // We resolve it against configDir. 
    // Or if it says "File.ttf", we might expect it in assetFolderPath/fonts/
    
    auto resolveFont = [&](std::string fontFile, std::string defaultFile) -> std::string {
        if (fontFile.empty()) fontFile = defaultFile;
        fs::path fp = fontFile;
        if (fp.is_absolute()) return fp.string();
        
        // Try direct resolve against config dir
        if (fs::exists(configDir / fp)) return (configDir / fp).string();
        
        // Try inside assetFolderPath/fonts
        if (fs::exists(fs::path(cfg.assetFolderPath) / "fonts" / fp.filename())) {
            return (fs::path(cfg.assetFolderPath) / "fonts" / fp.filename()).string();
        }
        
        // Fallback to simple resolve
        return resolvePath(fontFile);
    };

    cfg.arabicFont.file = resolveFont(data["arabicFont"].value("file", ""), QuranData::defaultArabicFont);
    cfg.arabicFont.size = data["arabicFont"].value("size", 100);
    cfg.arabicFont.color = data["arabicFont"].value("color", "FFFFFF");

    json translationFontConfig = data.contains("translationFont") ? data["translationFont"] : json::object();
    bool translationFontFamilyOverridden =
        translationFontConfig.contains("family") &&
        translationFontConfig.value("family", "") != QuranData::defaultTranslationFontFamily;
    bool translationFontFileOverridden =
        translationFontConfig.contains("file") &&
        translationFontConfig.value("file", "") != QuranData::defaultTranslationFont;

    if (translationFontFamilyOverridden) {
        cfg.translationFont.family = translationFontConfig.value("family", QuranData::defaultTranslationFontFamily);
    } else {
        cfg.translationFont.family = QuranData::getTranslationFontFamily(cfg.translationId);
    }
    cfg.translationFont.size = translationFontConfig.value("size", 50);
    cfg.translationFont.color = translationFontConfig.value("color", "D3D3D3");
    cfg.translationFallbackFontFamily =
        data.value("translationFallbackFontFamily", QuranData::defaultTranslationFontFamily);
    
    // Auto-select translation font based on translation ID if overridden config not provided
    std::string transFontFile = translationFontConfig.value("file", "");
    if (!translationFontFileOverridden) {
        transFontFile = QuranData::getTranslationFont(cfg.translationId);
    }
    cfg.translationFont.file = resolveFont(transFontFile, QuranData::defaultTranslationFont);

    // Data paths
    cfg.quranWordByWordPath = resolvePath(data.value("quranWordByWordPath", "data/quran/qpc-hafs-word-by-word.json"));

    // Timing parameters
    cfg.introDuration = data.value("introDuration", 1.0);
    cfg.pauseAfterIntroDuration = data.value("pauseAfterIntroDuration", 0.5);
    cfg.introFadeOutMs = data.value("introFadeOutMs", 500);
    
    // Text animation parameters
    cfg.enableTextGrowth = data.value("enableTextGrowth", true);
    cfg.textGrowthThreshold = data.value("textGrowthThreshold", 100);
    cfg.maxGrowthFactor = data.value("maxGrowthFactor", 1.15);
    cfg.growthRateFactor = data.value("growthRateFactor", 0.05);
    
    // Fade parameters
    cfg.fadeDurationFactor = data.value("fadeDurationFactor", 0.2);
    cfg.minFadeDuration = data.value("minFadeDuration", 0.05);
    cfg.maxFadeDuration = data.value("maxFadeDuration", 0.1);
    
    // Text wrapping parameters
    cfg.textWrapThreshold = data.value("textWrapThreshold", 20);
    cfg.arabicMaxWidthFraction = data.value("arabicMaxWidthFraction", 0.95);
    cfg.translationMaxWidthFraction = data.value("translationMaxWidthFraction", 0.85);
    cfg.textHorizontalPadding = data.value("textHorizontalPadding", 0.05);
    cfg.textVerticalPadding = data.value("textVerticalPadding", 0.08);
    
    // Layout parameters
    cfg.verticalShift = data.value("verticalShift", 40.0);
    
    // Thumbnail parameters
    if (data.contains("thumbnailColors") && data["thumbnailColors"].is_array()) {
        for (const auto& color : data["thumbnailColors"]) {
            cfg.thumbnailColors.push_back(color.get<std::string>());
        }
    }
    cfg.thumbnailNumberPadding = data.value("thumbnailNumberPadding", 100);

    // Quality / encoder parameters
    cfg.qualityProfile = data.value("qualityProfile", "balanced");
    cfg.crf = data.contains("crf") ? data["crf"].get<int>() : -1;
    cfg.pixelFormat = data.value("pixelFormat", "");
    cfg.videoBitrate = data.value("videoBitrate", "");
    cfg.videoMaxRate = data.value("videoMaxRate", "");
    cfg.videoBufSize = data.value("videoBufSize", "");
    auto qualityProfiles = loadQualityProfiles(data);

    // CLI overrides
    if (options.reciterId != -1) cfg.reciterId = options.reciterId;
    if (options.translationId != -1) {
        cfg.translationId = options.translationId;
        // Re-resolve if ID changed via CLI and wasn't overridden in config
        if (!translationFontFileOverridden) {
             std::string f = QuranData::getTranslationFont(cfg.translationId);
             cfg.translationFont.file = resolveFont(f, QuranData::defaultTranslationFont);
        }
        if (!translationFontFamilyOverridden) {
            cfg.translationFont.family = QuranData::getTranslationFontFamily(cfg.translationId);
        }
    }
    cfg.translationIsRtl = QuranData::isTranslationRtl(cfg.translationId);
    if (!options.recitationMode.empty()) {
        if (options.recitationMode == "gapless") {
            cfg.recitationMode = RecitationMode::GAPLESS;
        } else if (options.recitationMode == "gapped") {
            cfg.recitationMode = RecitationMode::GAPPED;
        } else {
            std::cerr << "Warning: Unknown recitation mode '" << options.recitationMode << "', using config default." << std::endl;
        }
    }
    if (options.width != -1) cfg.width = options.width;
    if (options.height != -1) cfg.height = options.height;
    if (options.fps != -1) cfg.fps = options.fps;
    if (options.arabicFontSize != -1) cfg.arabicFont.size = options.arabicFontSize;
    if (options.translationFontSize != -1) cfg.translationFont.size = options.translationFontSize;
    if (options.textPaddingOverride >= 0.0) {
        cfg.textHorizontalPadding = std::clamp(options.textPaddingOverride, 0.0, 0.45);
    }
    
    cfg.enableTextGrowth = options.enableTextGrowth;

    if (!options.qualityProfile.empty()) cfg.qualityProfile = options.qualityProfile;
    applyQualityProfile(cfg, options, qualityProfiles);
    if (options.customCRF != -1) cfg.crf = options.customCRF;
    if (!options.pixelFormatOverride.empty()) cfg.pixelFormat = options.pixelFormatOverride;
    if (!options.videoBitrateOverride.empty()) cfg.videoBitrate = options.videoBitrateOverride;
    if (!options.videoMaxRateOverride.empty()) cfg.videoMaxRate = options.videoMaxRateOverride;
    if (!options.videoBufSizeOverride.empty()) cfg.videoBufSize = options.videoBufSizeOverride;

    if (cfg.crf <= 0) cfg.crf = 23;
    if (cfg.pixelFormat.empty()) cfg.pixelFormat = "yuv420p";

    return cfg;
}

void validateAssets(const AppConfig& config) {
    if (!fs::exists(config.assetBgVideo)) {
        throw std::runtime_error("Background video not found: " + config.assetBgVideo);
    }
    
    if (!fs::exists(config.arabicFont.file)) {
        throw std::runtime_error("Arabic font file not found: " + config.arabicFont.file);
    }
    if (!fs::exists(config.translationFont.file)) {
        throw std::runtime_error("Translation font file not found: " + config.translationFont.file);
    }
    
    if (!fs::exists(config.quranWordByWordPath)) {
        throw std::runtime_error("Quran word-by-word data not found: " + config.quranWordByWordPath);
    }
}
