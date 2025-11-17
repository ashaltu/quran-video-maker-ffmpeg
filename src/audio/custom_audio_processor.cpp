#include "audio/custom_audio_processor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

extern "C" {
#include <libavformat/avformat.h>
}

namespace fs = std::filesystem;

namespace {

fs::path make_temp_audio_path(const fs::path& baseDir,
                              const std::string& prefix,
                              const std::string& ext = ".m4a") {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return baseDir / (prefix + "_" + std::to_string(stamp) + ext);
}

void run_ffmpeg_command(const std::string& cmd) {
    int code = std::system(cmd.c_str());
    if (code != 0) {
        throw std::runtime_error("FFmpeg command failed: " + cmd);
    }
}

fs::path trim_audio_segment(const std::string& source,
                            double startSec,
                            double endSec,
                            const fs::path& audioDir,
                            const std::string& label) {
    fs::path output = make_temp_audio_path(audioDir, label);
    std::ostringstream cmd;
    cmd << "ffmpeg -y ";
    if (startSec > 0.0) {
        cmd << "-ss " << std::fixed << std::setprecision(3) << startSec << " ";
    }
    if (endSec > 0.0 && endSec > startSec) {
        cmd << "-to " << std::fixed << std::setprecision(3) << endSec << " ";
    }
    cmd << "-i \"" << source << "\" -c copy \"" << output.string() << "\"";
    run_ffmpeg_command(cmd.str());
    return output;
}

fs::path concat_audio_segments(const std::vector<std::string>& segments,
                               const fs::path& audioDir,
                               const std::string& label) {
    if (segments.empty()) {
        throw std::runtime_error("No audio segments provided for concatenation.");
    }
    if (segments.size() == 1) {
        return fs::path(segments.front());
    }
    fs::path output = make_temp_audio_path(audioDir, label);
    std::ostringstream cmd;
    cmd << "ffmpeg -y ";
    for (const auto& segment : segments) {
        cmd << "-i \"" << segment << "\" ";
    }
    cmd << "-filter_complex \"";
    for (size_t i = 0; i < segments.size(); ++i) {
        cmd << "[" << i << ":a]";
    }
    cmd << "concat=n=" << segments.size() << ":v=0:a=1[out]\" -map \"[out]\" \"" << output.string() << "\"";
    run_ffmpeg_command(cmd.str());
    return output;
}

} // namespace

namespace Audio {

double CustomAudioProcessor::probeDuration(const std::string& filepath) {
    AVFormatContext* format_context = nullptr;
    if (avformat_open_input(&format_context, filepath.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Warning: Could not open audio file " << filepath << " to get duration." << std::endl;
        return 0.0;
    }
    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        std::cerr << "Warning: Could not find stream info for " << filepath << " to get duration." << std::endl;
        avformat_close_input(&format_context);
        return 0.0;
    }
    double duration = static_cast<double>(format_context->duration) / AV_TIME_BASE;
    avformat_close_input(&format_context);
    return duration;
}

SplicePlan CustomAudioProcessor::buildSplicePlan(const std::vector<VerseData>& verses,
                                                 const CLIOptions& options) {
    SplicePlan plan;
    if (options.customAudioPath.empty() || verses.empty()) {
        return plan;
    }

    plan.hasBismillah = !verses.empty() && verses.front().verseKey == "1:1";
    double mainStart = std::numeric_limits<double>::infinity();
    double mainEnd = -std::numeric_limits<double>::infinity();

    for (const auto& verse : verses) {
        if (!verse.fromCustomAudio) continue;
        if (verse.verseKey == "1:1") {
            plan.bismillahFromCustomSource = true;
            plan.bismillahStartMs = verse.absoluteTimestampFromMs;
            plan.bismillahEndMs = verse.absoluteTimestampToMs;
            if (plan.sourceAudioPath.empty() && !verse.sourceAudioPath.empty()) {
                plan.sourceAudioPath = verse.sourceAudioPath;
            }
            continue;
        }
        mainStart = std::min(mainStart, static_cast<double>(verse.absoluteTimestampFromMs));
        mainEnd = std::max(mainEnd, static_cast<double>(verse.absoluteTimestampToMs));
        if (plan.sourceAudioPath.empty() && !verse.sourceAudioPath.empty()) {
            plan.sourceAudioPath = verse.sourceAudioPath;
        }
    }

    if (!std::isfinite(mainStart) || mainEnd <= mainStart || plan.sourceAudioPath.empty()) {
        return plan;
    }

    plan.enabled = true;
    plan.mainStartMs = mainStart;
    plan.mainEndMs = mainEnd;

    if (plan.hasBismillah) {
        const auto& bismVerse = verses.front();
        if (!plan.bismillahFromCustomSource) {
            plan.bismillahStartMs = bismVerse.absoluteTimestampFromMs;
            plan.bismillahEndMs = bismVerse.absoluteTimestampToMs;
        }
        plan.paddingOffsetMs = plan.bismillahFromCustomSource
            ? (plan.bismillahEndMs - plan.bismillahStartMs)
            : bismVerse.durationInSeconds * 1000.0;
    } else {
        plan.paddingOffsetMs = 0.0;
    }

    return plan;
}

void CustomAudioProcessor::spliceRange(std::vector<VerseData>& verses,
                                       const CLIOptions& options,
                                       const std::filesystem::path& audioDir) {
    SplicePlan plan = buildSplicePlan(verses, options);
    if (!plan.enabled) return;

    fs::path mainTrimmed = trim_audio_segment(plan.sourceAudioPath,
                                              plan.mainStartMs / 1000.0,
                                              plan.mainEndMs / 1000.0,
                                              audioDir,
                                              "custom_main");

    std::vector<std::string> segments;
    double bismDurationMs = 0.0;

    if (plan.hasBismillah) {
        const auto& bismVerse = verses.front();
        if (plan.bismillahFromCustomSource) {
            std::string bismSource = !bismVerse.sourceAudioPath.empty()
                ? bismVerse.sourceAudioPath
                : plan.sourceAudioPath;
            fs::path bismTrimmed = trim_audio_segment(
                bismSource,
                plan.bismillahStartMs / 1000.0,
                plan.bismillahEndMs / 1000.0,
                audioDir,
                "custom_bism");
            segments.push_back(bismTrimmed.string());
            bismDurationMs = plan.bismillahEndMs - plan.bismillahStartMs;
        } else {
            std::string existing =
                !bismVerse.sourceAudioPath.empty() ? bismVerse.sourceAudioPath : bismVerse.localAudioPath;
            segments.push_back(existing);
            bismDurationMs = bismVerse.durationInSeconds * 1000.0;
        }
    }

    segments.push_back(mainTrimmed.string());
    fs::path finalAudio = concat_audio_segments(segments, audioDir, "custom_splice");

    double offsetMs = plan.hasBismillah ? bismDurationMs : 0.0;

    for (size_t i = 0; i < verses.size(); ++i) {
        verses[i].localAudioPath = finalAudio.string();
        verses[i].sourceAudioPath = finalAudio.string();

        if (plan.hasBismillah && i == 0) {
            verses[i].timestampFromMs = 0;
            verses[i].timestampToMs = plan.bismillahFromCustomSource
                ? static_cast<int>(plan.bismillahEndMs - plan.bismillahStartMs)
                : static_cast<int>(bismDurationMs);
            verses[i].durationInSeconds = (verses[i].timestampToMs - verses[i].timestampFromMs) / 1000.0;
            verses[i].absoluteTimestampFromMs = verses[i].timestampFromMs;
            verses[i].absoluteTimestampToMs = verses[i].timestampToMs;
            continue;
        }

        if (!verses[i].fromCustomAudio) {
            continue;
        }

        double newStart = (verses[i].absoluteTimestampFromMs - plan.mainStartMs) + offsetMs;
        double newEnd = (verses[i].absoluteTimestampToMs - plan.mainStartMs) + offsetMs;
        verses[i].timestampFromMs = static_cast<int>(std::max(0.0, newStart));
        verses[i].timestampToMs = static_cast<int>(std::max(newStart + 1.0, newEnd));
        verses[i].durationInSeconds =
            std::max(0.001, (verses[i].timestampToMs - verses[i].timestampFromMs) / 1000.0);
        verses[i].absoluteTimestampFromMs = verses[i].timestampFromMs;
        verses[i].absoluteTimestampToMs = verses[i].timestampToMs;
    }
}

} // namespace Audio
