#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "types.h"
#include "config_loader.h"
#include "cache_utils.h"
#include "recitation_utils.h"
#include "localization_utils.h"
#include "subtitle_builder.h"
#include "timing_parser.h"
#include "text/text_layout.h"
#include "audio/custom_audio_processor.h"

namespace fs = std::filesystem;

VerseData makeSampleVerse() {
    VerseData verse;
    verse.verseKey = "1:1";
    verse.text = "بِسْمِ اللّٰهِ";
    verse.translation = "In the name of Allah";
    verse.durationInSeconds = 1.5;
    verse.localAudioPath = "";
    verse.timestampFromMs = 0;
    verse.timestampToMs = 1500;
    return verse;
}

fs::path getProjectRoot() {
    static const fs::path root = fs::absolute(fs::path(__FILE__)).parent_path().parent_path();
    return root;
}

bool isGitLfsPointerFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string firstLine;
    if (!std::getline(in, firstLine)) return false;
    return firstLine.rfind("version https://git-lfs.github.com/spec/v1", 0) == 0;
}

void ensureArchiveMaterialized(const fs::path& archive) {
    if (!isGitLfsPointerFile(archive)) return;

    const std::string target = archive.filename().string();
    auto run = [](const std::string& cmd, const std::string& errorMessage) {
        const int rc = std::system(cmd.c_str());
        if (rc != 0) {
            throw std::runtime_error(errorMessage);
        }
    };

    run("git lfs pull --exclude='' --include='" + target + "'",
        "Failed to download data archive via git-lfs");
    run("git lfs checkout '" + target + "'",
        "Failed to checkout data archive via git-lfs");

    if (isGitLfsPointerFile(archive)) {
        throw std::runtime_error("Git LFS did not materialize the data archive");
    }
}

void ensureDataArchiveExtracted() {
    const fs::path dataDir = getProjectRoot() / "data";
    if (fs::exists(dataDir)) return;

    const fs::path archive = getProjectRoot() / "data.tar";
    if (!fs::exists(archive)) {
        throw std::runtime_error("Missing data.tar archive required for unit tests");
    }

    ensureArchiveMaterialized(archive);

    const std::string command = "tar -xf \"" + archive.string() + "\" -C \"" + getProjectRoot().string() + "\"";
    const int result = std::system(command.c_str());
    if (result != 0 || !fs::exists(dataDir)) {
        throw std::runtime_error("Failed to extract Quran dataset for tests");
    }
}

void testConfigLoader() {
    CLIOptions opts;
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    assert(cfg.width > 0);
    assert(cfg.height > 0);
    assert(cfg.assetFolderPath == "assets");
}

void testCacheUtils() {
    std::string sanitized = CacheUtils::sanitizeLabel("1:1/r");
    assert(sanitized.find(':') == std::string::npos);
    std::string translation = CacheUtils::getTranslationText(1, "1:1");
    assert(!translation.empty());
}

void testLocalization() {
    CLIOptions opts;
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    std::string lang = LocalizationUtils::getLanguageCode(cfg);
    assert(!lang.empty());
    std::string name = LocalizationUtils::getLocalizedSurahName(1, lang);
    assert(!name.empty());
    std::string label = LocalizationUtils::getLocalizedSurahLabel(lang);
    assert(!label.empty());
}

void testRecitationUtils() {
    std::vector<VerseData> verses;
    VerseData v1 = makeSampleVerse();
    VerseData v2 = makeSampleVerse();
    v2.timestampFromMs = -500;
    v2.timestampToMs = 500;
    verses.push_back(v1);
    verses.push_back(v2);
    RecitationUtils::normalizeGaplessTimings(verses);
    assert(verses[1].timestampFromMs >= verses[0].timestampToMs);

    TimingEntry entry;
    entry.startMs = 0;
    entry.endMs = 1000;
    CLIOptions opts;
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    VerseData bismillah = RecitationUtils::buildBismillahFromTiming(entry, cfg, "audio.mp3");
    assert(bismillah.verseKey == "1:1");
    assert(bismillah.durationInSeconds > 0);
}

void testTimingParser() {
    std::string tmpFile = (fs::temp_directory_path() / "sample_timing.vtt").string();
    std::ofstream out(tmpFile);
    out << "WEBVTT\n\n";
    out << "1\n";
    out << "00:00:00.000 --> 00:00:02.000\n";
    out << "1. In the name of Allah\n";
    out << "\n";
    out.close();

    auto timings = TimingParser::parseTimingFile(tmpFile);
    assert(!timings.byKey.empty());
    assert(!timings.ordered.empty());
    fs::remove(tmpFile);
}

void testSubtitleBuilder() {
    CLIOptions opts;
    opts.surah = 1;
    opts.from = 1;
    opts.to = 1;
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    std::vector<VerseData> verses = {makeSampleVerse()};
    std::string assPath = SubtitleBuilder::buildAssFile(cfg, opts, verses, cfg.introDuration, cfg.pauseAfterIntroDuration);
    assert(fs::exists(assPath));
}

void testTextLayoutEngine() {
    CLIOptions opts;
    opts.surah = 2;
    opts.from = 282;
    opts.to = 282;
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    TextLayout::Engine engine(cfg);
    VerseData verse = makeSampleVerse();
    verse.text = std::string(600, 'a');
    verse.translation = std::string(600, 'b');
    verse.durationInSeconds = 5.0;
    auto layout = engine.layoutVerse(verse);
    assert(layout.baseArabicSize > 0);
    assert(layout.baseTranslationSize > 0);
    assert(layout.wrappedArabic.find("\\N") != std::string::npos);
    assert(layout.wrappedTranslation.find("\\N") != std::string::npos);
}

void testCustomAudioPlan() {
    CLIOptions opts;
    opts.customAudioPath = "custom.mp3";
    opts.from = 72;
    std::vector<VerseData> verses;
    VerseData bism = makeSampleVerse();
    bism.verseKey = "1:1";
    bism.fromCustomAudio = true;
    bism.absoluteTimestampFromMs = 0;
    bism.absoluteTimestampToMs = 1500;
    bism.sourceAudioPath = "custom.mp3";
    verses.push_back(bism);

    VerseData v1 = makeSampleVerse();
    v1.verseKey = "19:72";
    v1.fromCustomAudio = true;
    v1.absoluteTimestampFromMs = 60000;
    v1.absoluteTimestampToMs = 70000;
    v1.sourceAudioPath = "custom.mp3";
    verses.push_back(v1);

    VerseData v2 = makeSampleVerse();
    v2.verseKey = "19:73";
    v2.fromCustomAudio = true;
    v2.absoluteTimestampFromMs = 70000;
    v2.absoluteTimestampToMs = 82000;
    v2.sourceAudioPath = "custom.mp3";
    verses.push_back(v2);

    auto plan = Audio::CustomAudioProcessor::buildSplicePlan(verses, opts);
    assert(plan.enabled);
    assert(plan.hasBismillah);
    assert(plan.bismillahFromCustomSource);
    assert(plan.mainStartMs == 60000);
    assert(plan.mainEndMs == 82000);
}

int main() {
    fs::current_path(getProjectRoot());
    ensureDataArchiveExtracted();
    testConfigLoader();
    testCacheUtils();
    testLocalization();
    testRecitationUtils();
    testTimingParser();
    testSubtitleBuilder();
    testTextLayoutEngine();
    testCustomAudioPlan();
    std::cout << "All unit tests passed.\n";
    return 0;
}
