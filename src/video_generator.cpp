#include "video_generator.h"
#include "video_selector.h"
#include "r2_client.h"
#include "quran_data.h"
#include "audio/custom_audio_processor.h"
#include "interfaces/IProcessExecutor.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <random>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <cctype>
#include "subtitle_builder.h"
#include "localization_utils.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace fs = std::filesystem;

namespace {
    void emitProgressEvent(const std::string& stage,
                       const std::string& status,
                       double percent = -1.0,
                       double elapsedSeconds = -1.0,
                       double etaSeconds = -1.0,
                       const std::string& message = "") {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2);
    oss << "PROGRESS {\"stage\":\"" << stage << "\",\"status\":\"" << status << "\"";
    if (percent >= 0.0) oss << ",\"percent\":" << percent;
    if (elapsedSeconds >= 0.0) oss << ",\"elapsedSeconds\":" << elapsedSeconds;
    if (etaSeconds >= 0.0) oss << ",\"etaSeconds\":" << etaSeconds;
    if (!message.empty()) oss << ",\"message\":\"" << message << "\"";
    oss << "}";
    std::cout << oss.str() << std::endl;
}

void emitStageMessage(const std::string& stage,
                      const std::string& status,
                      const std::string& message) {
    emitProgressEvent(stage, status, -1.0, -1.0, -1.0, message);
}
}

// Normalize paths for ffmpeg arguments.
static std::string to_ffmpeg_path(const fs::path& p) {
    return p.generic_string(); // forward slashes are accepted on all platforms
}

// Escape characters that are significant to FFmpeg filter arguments (e.g., colons inside paths).
static std::string to_ffmpeg_filter_path(const fs::path& p) {
    std::string s = to_ffmpeg_path(p);
#ifdef _WIN32
    std::string out;
    out.reserve(s.size() * 2);
    for (char ch : s) {
        if (ch == ':') {
            out.append("\\:");
        } else if (ch == '\'') {
            out.append("\\'");
        } else {
            out.push_back(ch);
        }
    }
    return out;
#else
    return s;
#endif
}

void VideoGenerator::generateVideo(const CLIOptions& options, const AppConfig& config, const std::vector<VerseData>& verses, std::shared_ptr<Interfaces::IProcessExecutor> processExecutor) {
    std::string backgroundVideoPath = config.assetBgVideo;

    if (config.videoSelection.enableDynamicBackgrounds) {
        try {
            VideoSelector::Selector selector(
                config.videoSelection.themeMetadataPath,
                config.videoSelection.seed
            );
            
            auto themes = selector.getThemesForVerses(options.surah, options.from, options.to);
            std::cout << "Available themes: ";
            for (const auto& t : themes) std::cout << t << " ";
            std::cout << std::endl;
            
            VideoSelector::SelectionState state;
            std::string verseRange = std::to_string(options.surah) + ":" + 
                                    std::to_string(options.from) + "-" + 
                                    std::to_string(options.to);
            
            std::string selectedTheme = selector.selectTheme(themes, verseRange, state);
            std::cout << "Selected theme: " << selectedTheme << std::endl;
            
            R2::R2Config r2Config{
                config.videoSelection.r2Endpoint,
                config.videoSelection.r2AccessKey,
                config.videoSelection.r2SecretKey,
                config.videoSelection.r2Bucket
            };
            
            R2::Client r2Client(r2Config);
            auto availableVideos = r2Client.listVideosInTheme(selectedTheme);
            
            if (availableVideos.empty()) {
                std::cerr << "Warning: No videos found for theme '" << selectedTheme 
                        << "', using default background" << std::endl;
            } else {
                std::string selectedVideo = selector.selectVideoFromTheme(
                    selectedTheme, availableVideos, state
                );
                std::cout << "Selected video: " << selectedVideo << std::endl;
                
                fs::path localPath = fs::temp_directory_path() / "bg_videos" / fs::path(selectedVideo).filename();
                backgroundVideoPath = r2Client.downloadVideo(selectedVideo, localPath);
                std::cout << "Downloaded to: " << backgroundVideoPath << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Dynamic background selection failed: " << e.what() 
                    << ", using default background" << std::endl;
        }
    }

    try {
        std::cout << "\n=== Starting Video Rendering ===" << std::endl;
        
        double intro_duration = config.introDuration;
        double pause_after_intro_duration = config.pauseAfterIntroDuration;
        
        std::cout << "Generating subtitles..." << std::endl;
        if (options.emitProgress) emitStageMessage("subtitles", "running", "Generating subtitles");
        std::string ass_filename = SubtitleBuilder::buildAssFile(config, options, verses, intro_duration, pause_after_intro_duration);
        std::string ass_ffmpeg_path = to_ffmpeg_filter_path(fs::path(ass_filename));
        std::string fonts_ffmpeg_path = to_ffmpeg_filter_path(fs::absolute(config.assetFolderPath) / "fonts");
        if (options.emitProgress) emitStageMessage("subtitles", "completed", "Subtitles generated");

        double verses_duration = 0.0;
        double minTimestampSec = std::numeric_limits<double>::infinity();
        double maxTimestampSec = 0.0;
        for (const auto& verse : verses) {
            verses_duration += verse.durationInSeconds;
            minTimestampSec = std::min(minTimestampSec, verse.timestampFromMs / 1000.0);
            maxTimestampSec = std::max(maxTimestampSec, verse.timestampToMs / 1000.0);
        }
        if (!std::isfinite(minTimestampSec)) {
            minTimestampSec = 0.0;
        }
        double total_duration = intro_duration + pause_after_intro_duration + verses_duration;

        std::stringstream filter_spec;
        filter_spec << "[0:v]setpts=PTS-STARTPTS,scale=" << config.width << ":" << config.height;
        
        size_t at_pos = config.overlayColor.find('@');
        bool apply_overlay = true;
        if (at_pos != std::string::npos) {
            try {
                double alpha = std::stod(config.overlayColor.substr(at_pos + 1));
                if (alpha <= 0.0) apply_overlay = false;
            } catch(...) {}
        }
        
        if (apply_overlay) {
            filter_spec << ",drawbox=x=0:y=0:w=iw:h=ih:color=" << config.overlayColor << ":t=fill";
        }
        
        filter_spec << ",ass='" << ass_ffmpeg_path << "':fontsdir='" << fonts_ffmpeg_path << "'[v]";

        std::ostringstream video_codec;
        if (options.encoder == "hardware") {
            #if defined(__APPLE__)
                video_codec << "-c:v h264_videotoolbox ";
                const std::string hardwareBitrate = !config.videoBitrate.empty() ? config.videoBitrate : "3500k";
                video_codec << "-b:v " << hardwareBitrate << " ";
                if (!config.videoMaxRate.empty()) video_codec << "-maxrate " << config.videoMaxRate << " ";
                if (!config.videoBufSize.empty()) video_codec << "-bufsize " << config.videoBufSize << " ";
                video_codec << "-allow_sw 1";
                std::cout << "Using hardware encoder: h264_videotoolbox" << std::endl;
            #else
                video_codec << "-c:v libx264 -preset " << options.preset << " -crf " << config.crf << " ";
                if (!config.videoBitrate.empty()) video_codec << "-b:v " << config.videoBitrate << " ";
                if (!config.videoMaxRate.empty()) video_codec << "-maxrate " << config.videoMaxRate << " ";
                if (!config.videoBufSize.empty()) video_codec << "-bufsize " << config.videoBufSize << " ";
            #endif
        } else {
            video_codec << "-c:v libx264 -preset " << options.preset << " -crf " << config.crf << " ";
            if (!config.videoBitrate.empty()) video_codec << "-b:v " << config.videoBitrate << " ";
            if (!config.videoMaxRate.empty()) video_codec << "-maxrate " << config.videoMaxRate << " ";
            if (!config.videoBufSize.empty()) video_codec << "-bufsize " << config.videoBufSize << " ";
            std::cout << "Using software encoder: libx264 ('" << options.preset << "')" << std::endl;
        }

        std::stringstream final_cmd;
        final_cmd << "ffmpeg ";
        if (options.emitProgress) {
            final_cmd << "-progress pipe:1 -nostats -loglevel warning ";
        }
        final_cmd << "-y ";
        
        // Handle audio differently for gapped vs gapless
        if (config.recitationMode == RecitationMode::GAPLESS) {
            // For gapless: use single surah audio file with precise trimming
            // This is much more efficient than concat - one file read instead of N files
            if (verses.empty()) throw std::runtime_error("No verses to render");
            
            std::string audioPath;
            for (const auto& verse : verses) {
                if (verse.localAudioPath.empty()) continue;
                audioPath = verse.localAudioPath;
                if (verse.fromCustomAudio) break;
            }
            if (audioPath.empty()) throw std::runtime_error("No audio path found for gapless render");
            bool customClip = !verses.empty() && verses[0].fromCustomAudio;
            double startTime = customClip ? 0.0 : minTimestampSec;
            double endTime = customClip ? verses_duration : maxTimestampSec;
            double trimmedDuration = std::max(0.0, endTime - startTime);
            double measuredAudioDuration = customClip
                ? Audio::CustomAudioProcessor::probeDuration(audioPath)
                : trimmedDuration;
            double audioDuration = customClip
                ? std::max(measuredAudioDuration, verses_duration)
                : measuredAudioDuration;
            total_duration = intro_duration + pause_after_intro_duration + audioDuration;
            
            final_cmd
                      << "-stream_loop -1 -i \"" << to_ffmpeg_path(config.assetBgVideo) << "\" "
                      << "-f lavfi -t " << (intro_duration + pause_after_intro_duration) << " -i anullsrc=r=44100:cl=stereo ";
            if (!customClip) {
                final_cmd << "-ss " << startTime << " -t " << trimmedDuration << " ";
            }
            final_cmd << "-i \"" << to_ffmpeg_path(audioPath) << "\" "
                      << "-filter_complex \""
                      << "[0:v]setpts=PTS-STARTPTS,scale=" << config.width << ":" << config.height;
            
            if (apply_overlay) {
                final_cmd << ",drawbox=x=0:y=0:w=iw:h=ih:color=" << config.overlayColor << ":t=fill";
            }
            
            final_cmd << ",ass='" << ass_ffmpeg_path << "':fontsdir='" 
                      << fonts_ffmpeg_path << "'[v];"
                      << "[1:a][2:a]concat=n=2:v=0:a=1[a]\" "
                      << "-map \"[v]\" -map \"[a]\" "
                      << "-t " << total_duration << " ";
        } else {
            // For gapped: concatenate individual ayah audio files
            std::string concat_file_path = (fs::temp_directory_path() / "audiolist.txt").string();
            {
                std::ofstream concat_file(concat_file_path);
                if (!concat_file.is_open()) throw std::runtime_error("Failed to create audio list file.");
                for (const auto& verse : verses) {
                    concat_file << "file '" << to_ffmpeg_path(fs::absolute(verse.localAudioPath)) << "'\n";
                }
            }
            
            double totalVideoDuration = intro_duration + pause_after_intro_duration;
            for(const auto& verse : verses) totalVideoDuration += verse.durationInSeconds;
            total_duration = totalVideoDuration;
            
            final_cmd
                      << "-stream_loop -1 -i \"" << to_ffmpeg_path(config.assetBgVideo) << "\" "
                      << "-itsoffset " << (intro_duration + pause_after_intro_duration) << " "
                      << "-f concat -safe 0 -i \"" << to_ffmpeg_path(concat_file_path) << "\" "
                      << "-filter_complex \"" << filter_spec.str() << "\" "
                      << "-map \"[v]\" -map 1:a "
                      << "-t " << totalVideoDuration << " ";
        }

        final_cmd << video_codec.str() << " "
                  << "-c:a aac -b:a 128k "
                  << "-pix_fmt " << config.pixelFormat << " "
                  << "-movflags +faststart "
                  << "-threads 8 "
                  << "\"" << options.output << "\"";

        std::cout << "\nExecuting FFmpeg command:\n" << final_cmd.str() << std::endl << std::endl;
        
        if (options.emitProgress) {
            processExecutor->executeWithProgress(final_cmd.str(), total_duration);
        } else {
            int exit_code = processExecutor->execute(final_cmd.str());
            if (exit_code != 0) throw std::runtime_error("FFmpeg execution failed");
        }

        std::cout << "\n✅ Render complete! Video saved to: " << options.output << std::endl;

    } catch(const std::exception& e) {
        std::cerr << "❌ An error occurred during video generation: " << e.what() << std::endl;
    }
}

void VideoGenerator::generateThumbnail(const CLIOptions& options, const AppConfig& config, std::shared_ptr<Interfaces::IProcessExecutor> processExecutor) {
    try {
        std::string output_dir = fs::path(options.output).parent_path().string();
        std::string thumbnail_path = (fs::path(output_dir) / "thumbnail.jpeg").string();

        std::string language_code = LocalizationUtils::getLanguageCode(config);
        std::string localized_surah_label = LocalizationUtils::getLocalizedSurahLabel(language_code);
        std::string localized_surah_name = LocalizationUtils::getLocalizedSurahName(options.surah, language_code);
        std::string localized_reciter_name = LocalizationUtils::getLocalizedReciterName(config.reciterId, language_code);
        std::string localized_surah_number = LocalizationUtils::getLocalizedNumber(options.surah, language_code);

        auto with_fallback = [&](const std::string& text) {
            return SubtitleBuilder::applyLatinFontFallback(
                text,
                config.translationFallbackFontFamily,
                config.translationFont.family);
        };
        std::string rendered_label = with_fallback(localized_surah_label);
        std::string rendered_surah_name = with_fallback(localized_surah_name);
        std::string rendered_reciter_name = with_fallback(localized_reciter_name);
        std::string rendered_surah_number = with_fallback(localized_surah_number);

        std::vector<std::string> colors = config.thumbnailColors;
        if (colors.empty()) {
            colors = {
                "&HFFFFFF&", // White
                "&HC0C0C0&", // Silver
                "&H00D7FF&"  // Gold
            };
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, colors.size() - 1);

        auto pick_color = [&]() { return colors[dis(gen)]; };

        int base_font_size = config.translationFont.size;
        int scaled_font_size = static_cast<int>(base_font_size * (config.width * 0.7 / (base_font_size * 3.0)));
        if (scaled_font_size < base_font_size) scaled_font_size = base_font_size;
        int label_size = scaled_font_size / 3;
        int reciter_size = scaled_font_size / 3;

        std::uniform_int_distribution<> side_dis(0, 1);
        int padding = config.thumbnailNumberPadding;
        bool right_side = side_dis(gen) == 1;
        int number_x = right_side ? (config.width - padding) : padding;
        std::string align = right_side ? "9" : "7";

        std::string number_color = pick_color();
        int number_size = scaled_font_size * 0.5;

        fs::path ass_path = fs::temp_directory_path() / "thumbnail.ass";
        std::ofstream ass_file(ass_path);
        if (!ass_file.is_open()) throw std::runtime_error("Failed to create temporary ASS file.");

        ass_file << "[Script Info]\nTitle: Thumbnail\nScriptType: v4.00+\n";
        ass_file << "PlayResX: " << config.width << "\nPlayResY: " << config.height << "\n\n";

        ass_file << "[V4+ Styles]\n";
        ass_file << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n";
        ass_file << "Style: Label," << config.translationFont.family << "," << label_size 
                << "," << pick_color() << ",&H000000FF&, &H003333&, &H00000000&,1,0,0,0,100,100,0,0,1,3,1,3,10,10,10,-1\n";
        ass_file << "Style: Main," << config.translationFont.family << "," << scaled_font_size 
                << "," << pick_color() << ",&H000000FF&, &H000000&, &H00000000&,1,0,0,0,100,100,0,0,1,5,3,5,10,10,10,-1\n";
        ass_file << "Style: Reciter," << config.translationFont.family << "," << reciter_size 
                << "," << pick_color() << ",&H000000FF&, &H003333&, &H00000000&,1,0,0,0,100,100,0,0,1,3,1,3,10,10,10,-1\n";
        ass_file << "Style: Number," << config.translationFont.family << "," << number_size 
                << "," << number_color << ",&H000000FF&, &H003333&, &H00000000&,1,0,0,0,100,100,0,0,1,5,3,5,10,10,10,-1\n\n";

        ass_file << "[Events]\n";
        ass_file << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Label,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2 - scaled_font_size*0.6) << ")\\fad(0," << config.introFadeOutMs << ")}" << rendered_label << "\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Main,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2) << ")\\fad(0," << config.introFadeOutMs << ")}" << rendered_surah_name << "\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Reciter,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2 + scaled_font_size*0.6) << ")\\fad(0," << config.introFadeOutMs << ")}" << rendered_reciter_name << "\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Number,,0,0,0,,{\\an" << align << "\\pos(" << number_x << ",50)\\fad(0," << config.introFadeOutMs << ")}" << rendered_surah_number << "\n";
                
        ass_file.close();

        std::string fonts_dir = to_ffmpeg_filter_path(fs::absolute(config.assetFolderPath) / "fonts");

        std::stringstream cmd;
        cmd << "ffmpeg -y "
            << "-ss 0 "
            << "-i \"" << to_ffmpeg_path(config.assetBgVideo) << "\" "
            << "-vf \"ass='" << to_ffmpeg_filter_path(ass_path) << "':fontsdir='" << fonts_dir << "'\" "
            << "-frames:v 1 "
            << "-q:v 2 "
            << "\"" << thumbnail_path << "\"";

        int exit_code = processExecutor->execute(cmd.str());
        if (exit_code != 0) throw std::runtime_error("FFmpeg thumbnail generation failed");

        std::cout << "✅ Thumbnail saved to: " << thumbnail_path << std::endl;

    } catch(const std::exception& e) {
        std::cerr << "❌ An error occurred during thumbnail generation: " << e.what() << std::endl;
    }
}
