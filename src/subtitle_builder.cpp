#include "subtitle_builder.h"
#include "quran_data.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <future>
#include <cctype>
#include "localization_utils.h"
#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

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

int adaptive_font_size_arabic(const std::string& text, int base_size) {
    int word_count = std::count(text.begin(), text.end(), ' ') + 1;
    if (word_count < 10) return 100;
    if (word_count < 40) return 85;
    if (word_count < 100) return 75;
    return 50;
}

int adaptive_font_size_translation(const std::string& text, int base_size) {
    int length = text.size();
    if (length < 80) return base_size;
    if (length < 160) return base_size * 0.85;
    if (length < 240) return base_size * 0.7;
    return base_size * 0.6;
}

bool should_grow(const std::string& arabic_text, int growth_threshold) {
    int word_count = std::count(arabic_text.begin(), arabic_text.end(), ' ') + 1;
    return word_count < growth_threshold;
}

struct FontContext {
    FT_Library ft;
    FT_Face face;
    hb_font_t* hb_font;
};

FontContext init_font(const std::string& font_file, int font_size) {
    FontContext ctx;
    if (FT_Init_FreeType(&ctx.ft)) throw std::runtime_error("Failed to init FreeType");
    if (FT_New_Face(ctx.ft, font_file.c_str(), 0, &ctx.face)) throw std::runtime_error("Failed to load font");
    FT_Set_Char_Size(ctx.face, 0, font_size*64, 0, 0);
    ctx.hb_font = hb_ft_font_create(ctx.face, nullptr);
    return ctx;
}

double measure_text_width(FontContext& ctx, const std::string& text) {
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(ctx.hb_font, buf, nullptr, 0);

    unsigned int glyph_count;
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
    double width = 0.0;
    for (unsigned int i = 0; i < glyph_count; i++) width += glyph_pos[i].x_advance / 64.0;

    hb_buffer_destroy(buf);
    return width;
}

void free_font(FontContext& ctx) {
    hb_font_destroy(ctx.hb_font);
    FT_Done_Face(ctx.face);
    FT_Done_FreeType(ctx.ft);
}

std::vector<std::string> split_ass_lines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t pos = text.find("\\N", start);
        if (pos == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, pos - start));
        start = pos + 2;
    }
    if (lines.empty()) {
        lines.push_back(text);
    }
    return lines;
}

std::string wrap_single_line(const std::string& line, FontContext& ctx, double max_width) {
    if (line.empty() || measure_text_width(ctx, line) <= max_width) {
        return line;
    }

    std::istringstream iss(line);
    std::string word;
    std::string current;
    std::vector<std::string> wrapped_lines;

    auto flush_current = [&]() {
        if (!current.empty()) {
            wrapped_lines.push_back(current);
            current.clear();
        }
    };

    while (iss >> word) {
        std::string candidate = current.empty() ? word : current + " " + word;
        if (measure_text_width(ctx, candidate) <= max_width || current.empty()) {
            current = candidate;
        } else {
            flush_current();
            current = word;
        }
    }
    flush_current();

    if (wrapped_lines.empty()) {
        wrapped_lines.push_back(line);
    }

    std::string rebuilt;
    for (size_t i = 0; i < wrapped_lines.size(); ++i) {
        rebuilt += wrapped_lines[i];
        if (i + 1 < wrapped_lines.size()) rebuilt += "\\N";
    }
    return rebuilt;
}

std::string wrap_if_too_wide_cached(const std::string& text, FontContext& ctx, double max_width) {
    auto lines = split_ass_lines(text);
    bool wrapping_applied = false;
    for (auto& line : lines) {
        double width = measure_text_width(ctx, line);
        if (width > max_width) {
            line = wrap_single_line(line, ctx, max_width);
            wrapping_applied = true;
        }
    }

    if (!wrapping_applied) {
        return text;
    }

    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i + 1 < lines.size()) result += "\\N";
    }
    return result;
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
    std::string localized_surah_text_render =
        applyLatinFontFallback(localized_surah_text,
                               QuranData::defaultTranslationFontFamily,
                               config.translationFont.family);

    ass_file << "[Script Info]\nTitle: Quran Video Subtitles\nScriptType: v4.00+\n";
    ass_file << "PlayResX: " << config.width << "\nPlayResY: " << config.height << "\n\n";

    double paddingFraction = std::clamp(config.textHorizontalPadding, 0.0, 0.45);
    double paddingPixels = config.width * paddingFraction;
    int styleMargin = std::max(10, static_cast<int>(paddingPixels));
    double arabicWrapWidth = std::max(50.0, (config.width - 2.0 * paddingPixels) * config.arabicMaxWidthFraction);
    double translationWrapWidth = std::max(50.0, (config.width - 2.0 * paddingPixels) * config.translationMaxWidthFraction);

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
            << "\\b1\\bord4\\shad3\\be2\\c&HFFD700&\\3c&H000000&"
            << "\\fad(0," << config.introFadeOutMs << ")}" << localized_surah_text_render << "\n";

    ass_file << "Dialogue: 0,0:00:00.00," << format_time_ass(intro_duration)
            << ",Translation,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << config.height/2 + scaled_font_size*1.5 << ")"
            << "\\fs" << scaled_font_size/2
            << "\\b0\\bord2\\shad1\\be1\\c&HFFFFFF&\\3c&H000000&"
            << "\\fad(0," << config.introFadeOutMs << ")}"
            << LocalizationUtils::getLocalizedNumber(options.surah, language_code)
            << " • " << options.from << "-" << options.to << "\n";

    // Process verses in parallel
    std::vector<VerseData> processed_verses(verses.size());
    std::vector<std::future<void>> futures;
    size_t max_threads = std::thread::hardware_concurrency();
    
    for (size_t i = 0; i < verses.size(); ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            processed_verses[i] = verses[i];
            int arabic_size = adaptive_font_size_arabic(processed_verses[i].text, config.arabicFont.size);
            int arabic_word_count = std::count(processed_verses[i].text.begin(), processed_verses[i].text.end(), ' ') + 1;
            bool growArabic = config.enableTextGrowth && should_grow(processed_verses[i].text, config.textWrapThreshold);
            double arabicGrowthFactor = growArabic
                ? std::min(config.maxGrowthFactor, 1.0 + processed_verses[i].durationInSeconds * config.growthRateFactor)
                : 1.0;
            int maxArabicSize = std::max(1, static_cast<int>(arabic_size * arabicGrowthFactor));
            
            FontContext arabic_ctx = init_font(fs::path(config.arabicFont.file), maxArabicSize);
            processed_verses[i].text = wrap_if_too_wide_cached(processed_verses[i].text, arabic_ctx, arabicWrapWidth);
            free_font(arabic_ctx);

            int translation_size = adaptive_font_size_translation(processed_verses[i].translation, config.translationFont.size);
            double translationGrowthFactor = growArabic
                ? std::min(config.maxGrowthFactor, 1.0 + processed_verses[i].durationInSeconds * config.growthRateFactor)
                : 1.0;
            int maxTranslationSize = std::max(1, static_cast<int>(translation_size * translationGrowthFactor));
            FontContext translation_ctx = init_font(fs::path(config.translationFont.file), maxTranslationSize);
            processed_verses[i].translation = wrap_if_too_wide_cached(processed_verses[i].translation, translation_ctx, translationWrapWidth);
            free_font(translation_ctx);
        }));
        
        if (futures.size() >= max_threads) {
            for (auto& f : futures) f.get();
            futures.clear();
        }
    }
    
    for (auto& f : futures) f.get();

    double cumulative_time = intro_duration + pause_after_intro_duration;
    for (const auto& verse_ref : processed_verses) {
        VerseData verse = verse_ref;
        int arabic_size = adaptive_font_size_arabic(verse.text, config.arabicFont.size);
        int translation_size = adaptive_font_size_translation(verse.translation, config.translationFont.size);

        double max_total_height = config.height * 0.8;
        double estimated_height = arabic_size * 1.2 + translation_size * 1.4;
        if (estimated_height > max_total_height) {
            double scale_factor = max_total_height / estimated_height;
            arabic_size = static_cast<int>(arabic_size * scale_factor);
            translation_size = static_cast<int>(translation_size * scale_factor);
        }

        int arabic_word_count = std::count(verse.text.begin(), verse.text.end(), ' ') + 1;
        bool grow = config.enableTextGrowth && should_grow(verse.text, config.textGrowthThreshold);

        double vertical_shift = config.verticalShift;
        double total_height = arabic_size * 1.2 + translation_size * 1.4;
        double arabic_y = config.height/2.0 - total_height*0.25 + vertical_shift;
        double translation_y = config.height/2.0 + total_height*0.25 + vertical_shift;

        arabic_y = std::max(arabic_y, static_cast<double>(arabic_size * 1.5));
        translation_y = std::min(translation_y, static_cast<double>(config.height - translation_size*2));

        double fade_time = std::min(std::max(verse.durationInSeconds * config.fadeDurationFactor, config.minFadeDuration), config.maxFadeDuration);
        double growth_factor = (arabic_word_count >= config.textGrowthThreshold) ? 1.0 : std::min(config.maxGrowthFactor, 1.0 + verse.durationInSeconds * config.growthRateFactor);

        std::stringstream combined;
        combined << "{\\an5\\q2\\rArabic"
                << "\\fs" << arabic_size
                << "\\pos(" << config.width/2 << "," << arabic_y << ")"
                << "\\fad(" << (fade_time*1000) << "," << (fade_time*1000) << ")";
        if (grow) {
            combined << "\\t(0," << verse.durationInSeconds*1000 << ",\\fs" << arabic_size*growth_factor << ")";
        }
        combined << "}" << verse.text
                << "\\N{\\an5\\q2\\rTranslation"
                << "\\fs" << translation_size
                << "\\pos(" << config.width/2 << "," << translation_y << ")"
                << "\\fad(" << (fade_time*1000) << "," << (fade_time*1000) << ")";
        if (grow) {
            combined << "\\t(0," << verse.durationInSeconds*1000 << ",\\fs" << translation_size*growth_factor << ")";
        }
        combined << "}" << verse.translation;

        ass_file << "Dialogue: 0," << format_time_ass(cumulative_time) << ","
                << format_time_ass(cumulative_time + verse.durationInSeconds)
                << ",Translation,,0,0,0,," << combined.str() << "\n";

        cumulative_time += verse.durationInSeconds;
    }

    return ass_path.string();
}

} // namespace SubtitleBuilder
