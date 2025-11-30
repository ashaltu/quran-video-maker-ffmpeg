#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "types.h"
#include "config_loader.h"
#include "cache_utils.h"
#include "recitation_utils.h"
#include "localization_utils.h"
#include "subtitle_builder.h"
#include "timing_parser.h"
#include "text/text_layout.h"
#include "audio/custom_audio_processor.h"
#include "api.h"
#include "metadata_writer.h"
#include "video_generator.h"

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

void testApi() {
    CLIOptions opts;
    opts.surah = 1;
    opts.from = 1;
    opts.to = 1;
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    std::vector<VerseData> verses = API::fetchQuranData(opts, cfg, (getProjectRoot() / "tests/mock_api_response.json").string());
    assert(verses.size() == 1);
    assert(verses[0].verseKey == "1:1");
    assert(!verses[0].text.empty());
}

void testMetadataWriter() {
    CLIOptions opts;
    opts.surah = 1;
    opts.from = 1;
    opts.to = 7;
    opts.output = (fs::temp_directory_path() / "test_video.mp4").string();
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    std::vector<std::string> rawArgs = {"quran-video-generator", "-s", "1", "-f", "1", "-t", "7", "-o", opts.output};
    MetadataWriter::writeMetadata(opts, cfg, rawArgs);
    assert(fs::exists(opts.output + ".metadata"));
    assert(fs::file_size(opts.output + ".metadata") > 0);
    fs::remove(opts.output + ".metadata");
}

void testVideoGenerator() {
    CLIOptions opts;
    opts.surah = 1;
    opts.from = 1;
    opts.to = 1;
    opts.output = (fs::temp_directory_path() / "test_video.mp4").string();
    AppConfig cfg = loadConfig((getProjectRoot() / "config.json").string(), opts);
    std::vector<VerseData> verses = {makeSampleVerse()};
    std::string dummyAudioPath = (fs::temp_directory_path() / "dummy.wav").string();
    std::ofstream dummyAudio(dummyAudioPath, std::ios::binary);
    // Write a minimal WAV header for a silent audio file
    dummyAudio.write("RIFF", 4);
    dummyAudio.write("\x24\x00\x00\x00", 4); // ChunkSize
    dummyAudio.write("WAVE", 4);
    dummyAudio.write("fmt ", 4);
    dummyAudio.write("\x10\x00\x00\x00", 4); // Subchunk1Size
    dummyAudio.write("\x01\x00", 2);       // AudioFormat
    dummyAudio.write("\x01\x00", 2);       // NumChannels
    dummyAudio.write("\x44\xAC\x00\x00", 4); // SampleRate
    dummyAudio.write("\x88\x58\x01\x00", 4); // ByteRate
    dummyAudio.write("\x02\x00", 2);       // BlockAlign
    dummyAudio.write("\x10\x00", 2);       // BitsPerSample
    dummyAudio.write("data", 4);
    dummyAudio.write("\x00\x00\x00\x00", 4); // Subchunk2Size
    dummyAudio.close();
    verses[0].localAudioPath = dummyAudioPath;

    std::string videoCommandPath = (fs::temp_directory_path() / "video_command.txt").string();
    VideoGenerator::generateVideo(opts, cfg, verses, videoCommandPath);
    std::ifstream videoCommandFile(videoCommandPath);
    std::stringstream videoCommand;
    videoCommand << videoCommandFile.rdbuf();
    videoCommandFile.close();
    assert(videoCommand.str().find("ffmpeg") != std::string::npos);
    assert(videoCommand.str().find(opts.output) != std::string::npos);

    std::string thumbCommandPath = (fs::temp_directory_path() / "thumb_command.txt").string();
    VideoGenerator::generateThumbnail(opts, cfg, thumbCommandPath);
    std::ifstream thumbCommandFile(thumbCommandPath);
    std::stringstream thumbCommand;
    thumbCommand << thumbCommandFile.rdbuf();
    thumbCommandFile.close();
    assert(thumbCommand.str().find("ffmpeg") != std.string::npos);
    std::string thumbPath = (fs::path(opts.output).parent_path() / "thumbnail.jpeg").string();
    assert(thumbCommand.str().find(thumbPath) != std.string::npos);

    fs::remove(opts.output);
    fs::remove(videoCommandPath);
    fs::remove(thumbCommandPath);
    fs::remove(dummyAudioPath);
}

int main() {
    fs::current_path(getProjectRoot());
    testVideoGenerator();
    testApi();
    testMetadataWriter();
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
