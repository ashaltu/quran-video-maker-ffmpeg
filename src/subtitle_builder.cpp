#include "subtitle_builder.h"
#include "quran_data.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <future>
#include <cctype>
#include <algorithm>
#include "localization_utils.h"
#include "text/text_layout.h"

namespace fs = std::filesystem;

namespace {

std::string format_time_ass(double seconds) {
    int hours = seconds / 3600;
    seconds -= hours * 3600;
    int minutes = seconds / 60;
    seconds -= minutes * 60;
    int secs = seconds;
    int centiseconds = (seconds - secs) * 100;

    std::stringstream ss;
    ss << hours << ":" << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << secs << "."
       << std::setfill('0') << std::setw(2) << centiseconds;
    return ss.str();
}

std::string format_ass_color(const std::string& hex_color) {
    std::string clean_hex = hex_color;
    if (clean_hex.rfind("#", 0) == 0) {
        clean_hex = clean_hex.substr(1);
    }
    return "&H" + clean_hex + "&";
}

bool is_basic_latin_ascii(unsigned char c) {
    return c >= 0x20 && c <= 0x7E;
}

} // namespace

namespace SubtitleBuilder {

std::string applyLatinFontFallback(const std::string& text,
                                   const std::string& fallbackFont,
                                   const std::string& primaryFont) {
    if (fallbackFont.empty() || fallbackFont == primaryFont) return text;

    bool hasLatin = false;
    for (unsigned char c : text) {
        if (is_basic_latin_ascii(c)) {
            hasLatin = true;
            break;
        }
    }
    if (!hasLatin) return text;

    std::string result;
    bool usingFallback = false;

    auto append_font_tag = [&](const std::string& font) {
        result += "{\\fn" + font + "}";
    };

    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t len = 1;
        if ((c & 0xF8) == 0xF0) {
            len = 4;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        }

        bool isLatin = (len == 1) && is_basic_latin_ascii(c);
        if (isLatin && !usingFallback) {
            append_font_tag(fallbackFont);
            usingFallback = true;
        } else if (!isLatin && usingFallback) {
            append_font_tag(primaryFont);
            usingFallback = false;
        }

        result.append(text, i, len);
        i += len;
    }

    if (usingFallback) {
        append_font_tag(primaryFont);
    }
    return result;
}

std::string buildAssFile(const AppConfig& config,
                         const CLIOptions& options,
                         const std::vector<VerseData>& verses,
                         double intro_duration,
                         double pause_after_intro_duration) {
    fs::path ass_path = fs::temp_directory_path() / "subtitles.ass";
    std::ofstream ass_file(ass_path);
    if (!ass_file.is_open()) throw std::runtime_error("Failed to create temporary subtitle file.");

    std::string language_code = LocalizationUtils::getLanguageCode(config);
    std::string localized_surah_name = LocalizationUtils::getLocalizedSurahName(options.surah, language_code);
    std::string localized_surah_label = LocalizationUtils::getLocalizedSurahLabel(language_code);
    std::string localized_surah_text = localized_surah_label + " " + localized_surah_name;

    if (config.translationIsRtl) {
        localized_surah_text = LocalizationUtils::reverseWords(localized_surah_text);
    }

    std::string localized_surah_text_render =
        applyLatinFontFallback(localized_surah_text,
                               config.translationFallbackFontFamily,
                               config.translationFont.family);

    ass_file << "[Script Info]\nTitle: Quran Video Subtitles\nScriptType: v4.00+\n";
    ass_file << "PlayResX: " << config.width << "\nPlayResY: " << config.height << "\n\n";

    TextLayout::Engine layoutEngine(config);
    double paddingPixels = layoutEngine.paddingPixels();
    int styleMargin = std::max(10, static_cast<int>(paddingPixels));

    ass_file << "[V4+ Styles]\n";
    ass_file << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n";
    ass_file << "Style: Arabic," << config.arabicFont.family << "," << config.arabicFont.size << "," << format_ass_color(config.arabicFont.color) << ",&H000000FF,&H00000000,&H99000000,0,0,0,0,100,100,0,0,1,1,1,5," << styleMargin << "," << styleMargin << "," << config.arabicFont.size * 1.5 << ",1\n";
    ass_file << "Style: Translation," << config.translationFont.family << "," << config.translationFont.size << "," << format_ass_color(config.translationFont.color) << ",&H000000FF,&H00000000,&H99000000,0,0,0,0,100,100,0,0,1,1,1,5," << styleMargin << "," << styleMargin << "," << config.height / 2 + config.translationFont.size << ",1\n\n";
    ass_file << "[Events]\n";
    ass_file << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

    int base_font_size = config.translationFont.size;
    int scaled_font_size = static_cast<int>(base_font_size * (config.width * 0.7 / (base_font_size * 6.0)));
    if (scaled_font_size < base_font_size) scaled_font_size = base_font_size;

    ass_file << "Dialogue: 0,0:00:00.00," << format_time_ass(intro_duration)
            << ",Translation,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << config.height/2 << ")"
            << "\\fs" << scaled_font_size
            << "\\b1\\bord4\\shad3\\be2\\c&HFFFFFF&\\3c&H000000&"
            << "\\fad(0," << config.introFadeOutMs << ")}" << localized_surah_text_render << "\n";

    std::string range_text = LocalizationUtils::getLocalizedNumber(options.surah, language_code) +
                             " • " + std::to_string(options.from) + "-" + std::to_string(options.to);
    
    if (config.translationIsRtl) {
        range_text = LocalizationUtils::reverseWords(range_text);
    }
    
    range_text = applyLatinFontFallback(range_text,
                                        config.translationFallbackFontFamily,
                                        config.translationFont.family);

    ass_file << "Dialogue: 0,0:00:00.00," << format_time_ass(intro_duration)
            << ",Translation,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << config.height/2 + scaled_font_size*1.5 << ")"
            << "\\fs" << scaled_font_size/2
            << "\\b0\\bord2\\shad1\\be1\\c&HFFFFFF&\\3c&H000000&"
            << "\\fad(0," << config.introFadeOutMs << ")}"
            << range_text << "\n";

    // Process verses in parallel
    std::vector<VerseData> processed_verses(verses.size());
    std::vector<TextLayout::LayoutResult> layout_results(verses.size());
    std::vector<std::future<void>> futures;
    size_t max_threads = std::max<size_t>(1, std::thread::hardware_concurrency());

    for (size_t i = 0; i < verses.size(); ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            processed_verses[i] = verses[i];
            layout_results[i] = layoutEngine.layoutVerse(verses[i]);
            processed_verses[i].text = layout_results[i].wrappedArabic;
            processed_verses[i].translation = layout_results[i].wrappedTranslation;
        }));

        if (futures.size() >= max_threads) {
            for (auto& f : futures) f.get();
            futures.clear();
        }
    }

    for (auto& f : futures) f.get();

    double cumulative_time = intro_duration + pause_after_intro_duration;
    double verticalPadding = config.height * std::clamp(config.textVerticalPadding, 0.0, 0.3);

    for (size_t idx = 0; idx < processed_verses.size(); ++idx) {
        VerseData verse = processed_verses[idx];
        const auto& info = layout_results[idx];

        int arabic_size = info.baseArabicSize;
        std::string translation_rendered = applyLatinFontFallback(
            verse.translation, config.translationFallbackFontFamily, config.translationFont.family);
        int translation_size = info.baseTranslationSize;

        double max_total_height = config.height * 0.8;
        double estimated_height = arabic_size * 1.2 + translation_size * 1.4;
        if (estimated_height > max_total_height) {
            double scale_factor = max_total_height / estimated_height;
            arabic_size = static_cast<int>(arabic_size * scale_factor);
            translation_size = static_cast<int>(translation_size * scale_factor);
        }

        bool grow = info.growArabic;
        double growth_factor = info.arabicGrowthFactor;
        double translation_growth = info.translationGrowthFactor;

        double vertical_shift = config.verticalShift;
        double total_height = arabic_size * 1.2 + translation_size * 1.4;
        double arabic_y = config.height / 2.0 - total_height * 0.25 + vertical_shift;
        double translation_y = config.height / 2.0 + total_height * 0.25 + vertical_shift;

        double minArabicY = verticalPadding + arabic_size * 1.1;
        double maxTranslationY = config.height - verticalPadding - translation_size * 1.1;
        arabic_y = std::max(arabic_y, minArabicY);
        translation_y = std::min(translation_y, maxTranslationY);
        if (translation_y - arabic_y < translation_size * 1.2) {
            translation_y = std::min(maxTranslationY, arabic_y + translation_size * 1.2);
        }

        double fade_time = std::min(
            std::max(verse.durationInSeconds * config.fadeDurationFactor, config.minFadeDuration),
            config.maxFadeDuration);

        std::stringstream combined;
        combined << "{\\an5\\q2\\rArabic"
                 << "\\fs" << arabic_size
                 << "\\pos(" << config.width / 2 << "," << arabic_y << ")"
                 << "\\fad(" << (fade_time * 1000) << "," << (fade_time * 1000) << ")";
        if (grow) {
            combined << "\\t(0," << verse.durationInSeconds * 1000 << ",\\fs" << arabic_size * growth_factor << ")";
        }
        combined << "}" << verse.text
                 << "\\N{\\an5\\q2\\rTranslation"
                 << "\\fs" << translation_size
                 << "\\pos(" << config.width / 2 << "," << translation_y << ")"
                 << "\\fad(" << (fade_time * 1000) << "," << (fade_time * 1000) << ")";
        if (translation_growth > 1.0) {
            combined << "\\t(0," << verse.durationInSeconds * 1000 << ",\\fs"
                     << translation_size * translation_growth << ")";
        }
        combined << "}" << translation_rendered;

        ass_file << "Dialogue: 0," << format_time_ass(cumulative_time) << ","
                 << format_time_ass(cumulative_time + verse.durationInSeconds)
                 << ",Translation,,0,0,0,," << combined.str() << "\n";

        cumulative_time += verse.durationInSeconds;
    }

    return ass_path.string();
}

} // namespace SubtitleBuilder
