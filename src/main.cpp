#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <vector>
#include "cxxopts.hpp"

#include "types.h"
#include "LiveApiClient.h"
#include "video_generator.h"
#include "quran_data.h"
#include "config_loader.h"
#include "SystemProcessExecutor.h"
#include <memory>
#include "metadata_writer.h"
#include "cache_utils.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::vector<std::string> invocationArgs(argv, argv + argc);
    cxxopts::Options cli_parser("QuranVideoMaker", "Generates Quran videos using FFmpeg");
    // TODO: fix so default values removed and values read from config instead
    cli_parser.add_options()
        ("surah", "Surah number", cxxopts::value<int>())
        ("from", "Starting verse", cxxopts::value<int>())
        ("to", "Ending verse", cxxopts::value<int>())
        ("c,config", "Path to config file", cxxopts::value<std::string>()->default_value("./config.json"))
        ("r,reciter", "Reciter ID", cxxopts::value<int>())
        ("t,translation", "Translation ID", cxxopts::value<int>())
        ("m,mode", "Recitation mode: 'gapped' (ayah-by-ayah) or 'gapless' (surah-by-surah)", cxxopts::value<std::string>())
        ("o,output", "Output filename", cxxopts::value<std::string>())
        ("width", "Video width", cxxopts::value<int>())
        ("height", "Video height", cxxopts::value<int>())
        ("fps", "Frames per second", cxxopts::value<int>())
        ("arabic-font-size", "Override Arabic font size", cxxopts::value<int>())
        ("translation-font-size", "Override translation font size", cxxopts::value<int>())
        ("text-padding", "Horizontal padding fraction (0-0.45) for subtitles", cxxopts::value<double>())
        ("e,encoder", "Choose encoder: 'software' (default) or 'hardware'", cxxopts::value<std::string>()->default_value("software"))
        ("p,preset", "Software encoder preset for speed/quality (ultrafast, fast, medium)", cxxopts::value<std::string>()->default_value("fast"))
        ("quality-profile", "Quality profile: speed | balanced | max", cxxopts::value<std::string>())
        ("crf", "Constant Rate Factor (0-51). Lower improves quality.", cxxopts::value<int>())
        ("pix-fmt", "Pixel format (e.g. yuv420p, yuv420p10le)", cxxopts::value<std::string>())
        ("video-bitrate", "Target video bitrate (e.g. 6000k)", cxxopts::value<std::string>())
        ("maxrate", "Maximum encoder bitrate (e.g. 8000k)", cxxopts::value<std::string>())
        ("bufsize", "Encoder buffer size (e.g. 12000k)", cxxopts::value<std::string>())
        ("no-cache", "Disable caching", cxxopts::value<bool>()->default_value("false"))
        ("clear-cache", "Clear all cached data", cxxopts::value<bool>()->default_value("false"))
        ("no-growth", "Disable text growth animations", cxxopts::value<bool>()->default_value("false"))
        ("progress", "Emit structured progress logs (PROGRESS ...)", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
        ("bg-theme", "Background video theme (space, nature, abstract, minimal)", cxxopts::value<std::string>())
        ("custom-audio", "Custom audio file path or URL (gapless mode only)", cxxopts::value<std::string>())
        ("custom-timing", "Custom timing file (VTT or SRT format)", cxxopts::value<std::string>())
        ("generate-backend-metadata,gbm", "Generate metadata for backend server and exit")
        ("h,help", "Print usage");
    
    cli_parser.parse_positional({"surah", "from", "to"});
    auto result = cli_parser.parse(argc, argv);

    if (result.count("generate-backend-metadata")) {
        if (!result.count("output")) {
            std::cerr << "Error: --output must be provided when using --generate-backend-metadata and must point to a .json file." << std::endl;
            return 1;
        }
        fs::path metadataOutput = result["output"].as<std::string>();
        if (metadataOutput.extension() != ".json") {
            std::cerr << "Error: --output path must have a .json extension for backend metadata generation." << std::endl;
            return 1;
        }
        try {
            MetadataWriter::generateBackendMetadata(metadataOutput.string());
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Fatal Error: " << e.what() << std::endl;
            return 1;
        }
    }

    if (result.count("help") || !result.count("surah") || !result.count("from") || !result.count("to")) {
        std::cout << cli_parser.help() << std::endl;
        std::cout << "\nRecitation Modes:\n"
                  << "  gapped  - Ayah-by-ayah with pauses between verses (default)\n"
                  << "  gapless - Continuous surah recitation with precise timing\n\n"
                  << "Custom Recitation (gapless mode only):\n"
                  << "  Use --custom-audio and --custom-timing together to specify:\n"
                  << "    --custom-audio <path|url>  - Path or URL to audio file\n"
                  << "    --custom-timing <file>     - VTT or SRT file with verse timings\n"
                  << "  Example:\n"
                  << "    --custom-audio ./my_recitation.mp3 --custom-timing ./timings.vtt\n\n";
        return 1;
    }

    const std::string gaplessDisabledError = "Error: Gapless mode is temporarily disabled because it's too buggy and the gapless data needs to be cleaned first.";

    CLIOptions options;
    options.surah = result["surah"].as<int>();
    options.from = result["from"].as<int>();
    options.to = result["to"].as<int>();
    options.configPath = result["config"].as<std::string>();
    options.configPathProvided = result.count("config") > 0;
    if (result.count("reciter")) options.reciterId = result["reciter"].as<int>();
    if (result.count("translation")) options.translationId = result["translation"].as<int>();
    if (result.count("mode")) options.recitationMode = result["mode"].as<std::string>();
    if (result.count("width")) options.width = result["width"].as<int>();
    if (result.count("height")) options.height = result["height"].as<int>();
    if (result.count("fps")) options.fps = result["fps"].as<int>();
    if (result.count("arabic-font-size")) options.arabicFontSize = result["arabic-font-size"].as<int>();
    if (result.count("translation-font-size")) options.translationFontSize = result["translation-font-size"].as<int>();
    options.noCache = result["no-cache"].as<bool>();
    options.clearCache = result["clear-cache"].as<bool>();
    options.preset = result["preset"].as<std::string>();
    options.presetProvided = result.count("preset");
    options.encoder = result["encoder"].as<std::string>();
    options.enableTextGrowth = !result["no-growth"].as<bool>();
    options.emitProgress = result["progress"].as<bool>();
    if (result.count("text-padding")) options.textPaddingOverride = result["text-padding"].as<double>();
    if (result.count("quality-profile")) options.qualityProfile = result["quality-profile"].as<std::string>();
    if (result.count("crf")) options.customCRF = result["crf"].as<int>();
    if (result.count("pix-fmt")) options.pixelFormatOverride = result["pix-fmt"].as<std::string>();
    if (result.count("video-bitrate")) options.videoBitrateOverride = result["video-bitrate"].as<std::string>();
    if (result.count("maxrate")) options.videoMaxRateOverride = result["maxrate"].as<std::string>();
    if (result.count("bufsize")) options.videoBufSizeOverride = result["bufsize"].as<std::string>();
    
    // Custom recitation options
    if (result.count("custom-audio")) options.customAudioPath = result["custom-audio"].as<std::string>();
    if (result.count("custom-timing")) options.customTimingFile = result["custom-timing"].as<std::string>();
    
    // Validate custom recitation usage
    if (!options.customAudioPath.empty() || !options.customTimingFile.empty()) {
        if (options.customAudioPath.empty() || options.customTimingFile.empty()) {
            std::cerr << "Error: Both --custom-audio and --custom-timing must be specified together." << std::endl;
            return 1;
        }
        // Custom recitations only work in gapless mode
        if (options.recitationMode.empty()) {
            options.recitationMode = "gapless";
        } else if (options.recitationMode != "gapless") {
            std::cerr << "Error: Custom recitations only work in gapless mode." << std::endl;
            return 1;
        }
    }

    // We want to allow gapless mode for custom audio
    if (options.recitationMode == "gapless" && options.customAudioPath.empty()) {
        std::cerr << gaplessDisabledError << std::endl;
        return 1;
    }
    
    if (result.count("output")) {
        options.output = result["output"].as<std::string>();
    } else {
        fs::path default_output_dir = "out";
        if (!fs::exists(default_output_dir)) {
            if (!fs::create_directories(default_output_dir)) {
                throw std::runtime_error("Failed to create directory: " + default_output_dir.string());
            }
        }
        options.output = "out/surah-" + std::to_string(options.surah) + "_" + std::to_string(options.from) + "-" + std::to_string(options.to) + ".mp4";
    }
    
    try {
        fs::path cacheDir = CacheUtils::getCacheRoot();
        if (options.clearCache && fs::exists(cacheDir)) {
            std::cout << "Clearing cache..." << std::endl;
            fs::remove_all(cacheDir);
        }
        
        AppConfig config = loadConfig(options.configPath, options);

        // We want to allow gapless mode for custom audio
        if (config.recitationMode == RecitationMode::GAPLESS && options.customAudioPath.empty()) {
            std::cerr << gaplessDisabledError << std::endl;
            return 1;
        }
        
        // Override background theme if specified
        if (result.count("bg-theme")) {
            std::string theme = result["bg-theme"].as<std::string>();
            auto it = QuranData::backgroundThemes.find(theme);
            if (it != QuranData::backgroundThemes.end()) {
                fs::path themePath = it->second;
                if (!themePath.is_absolute()) {
                    themePath = fs::path(config.assetFolderPath) / themePath;
                }
                config.assetBgVideo = themePath.string();
            } else {
                std::cerr << "Warning: Unknown theme '" << theme << "', using default." << std::endl;
            }
        }
        
        validateAssets(config);

        std::string modeStr = (config.recitationMode == RecitationMode::GAPLESS) ? "gapless" : "gapped";
        std::cout << "Rendering Surah " << options.surah << ", verses " << options.from << "-" << options.to << std::endl;
        std::cout << "Mode: " << modeStr << std::endl;
        std::cout << "Config: " << config.width << "x" << config.height << " @ " << config.fps << "fps, reciter=" << config.reciterId << ", translation=" << config.translationId << std::endl;
        std::cout << "Text growth: " << (config.enableTextGrowth ? "enabled" : "disabled") << std::endl;

        auto processExecutor = std::make_shared<SystemProcessExecutor>();
        auto apiClient = std::make_shared<LiveApiClient>();
        auto verses = apiClient->fetchQuranData(options, config);
        MetadataWriter::writeMetadata(options, config, invocationArgs);
        VideoGenerator::generateVideo(options, config, verses, processExecutor);
        VideoGenerator::generateThumbnail(options, config, processExecutor);

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
