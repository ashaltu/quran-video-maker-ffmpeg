#include "config_loader.h"
#include "quran_data.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using json = nlohmann::json;

AppConfig loadConfig(const std::string& path, CLIOptions& options) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Could not open config file: " + path);
    
    json data = json::parse(f);
    AppConfig cfg;

    // Video dimensions
    cfg.width = data.value("width", 1280);
    cfg.height = data.value("height", 720);
    cfg.fps = data.value("fps", 30);
    
    // Content selection
    cfg.reciterId = data.value("reciterId", 7);
    cfg.translationId = data.value("translationId", 1);
    
    // Recitation mode
    std::string modeStr = data.value("recitationMode", "gapped");
    if (modeStr == "gapless") {
        cfg.recitationMode = RecitationMode::GAPLESS;
    } else {
        cfg.recitationMode = RecitationMode::GAPPED;
    }
    
    // Visual styling
    cfg.overlayColor = data.value("overlayColor", "0x000000@0.5");
    cfg.assetFolderPath = data.value("assetFolderPath", "assets");
    cfg.assetBgVideo = cfg.assetFolderPath + "/" + data.value("assetBgVideo", QuranData::defaultBackgroundVideo);

    // Font configuration
    cfg.arabicFont.family = data["arabicFont"].value("family", "KFGQPC HAFS Uthmanic Script");
    cfg.arabicFont.file = cfg.assetFolderPath + "/" + data["arabicFont"].value("file", QuranData::defaultArabicFont);
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
    
    // Auto-select translation font based on translation ID if overridden config not provided
    if (translationFontFileOverridden) {
        cfg.translationFont.file = cfg.assetFolderPath + "/" + translationFontConfig.value("file", QuranData::defaultTranslationFont);
    } else {
        cfg.translationFont.file = cfg.assetFolderPath + "/" + QuranData::getTranslationFont(cfg.translationId);
    }

    // Data paths
    cfg.quranWordByWordPath = data.value("quranWordByWordPath", "data/quran/qpc-hafs-word-by-word.json");

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
    
    // Layout parameters
    cfg.verticalShift = data.value("verticalShift", 40.0);
    
    // Thumbnail parameters
    if (data.contains("thumbnailColors") && data["thumbnailColors"].is_array()) {
        for (const auto& color : data["thumbnailColors"]) {
            cfg.thumbnailColors.push_back(color.get<std::string>());
        }
    }
    cfg.thumbnailNumberPadding = data.value("thumbnailNumberPadding", 100);

    // CLI overrides
    if (options.reciterId != -1) cfg.reciterId = options.reciterId;
    if (options.translationId != -1) {
        cfg.translationId = options.translationId;
        cfg.translationFont.file = cfg.assetFolderPath + "/" + QuranData::getTranslationFont(cfg.translationId);
        if (!translationFontFamilyOverridden) {
            cfg.translationFont.family = QuranData::getTranslationFontFamily(cfg.translationId);
        }
    }
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
    
    cfg.enableTextGrowth = options.enableTextGrowth;

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
