#include "text/text_layout.h"
#include "localization_utils.h"

#include <algorithm>
#include <filesystem>
#include <future>
#include <sstream>
#include <string>
#include <vector>

#include <hb-ft.h>
#include <hb.h>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace fs = std::filesystem;

namespace {

int count_words(const std::string& text) {
    int words = 0;
    bool inWord = false;
    for (unsigned char c : text) {
        if (std::isspace(c)) {
            if (inWord) {
                ++words;
                inWord = false;
            }
        } else {
            inWord = true;
        }
    }
    if (inWord) ++words;
    return std::max(words, 1);
}

int adaptive_font_size_arabic(const std::string& text, int base_size) {
    const int word_count = count_words(text);
    base_size = std::max(base_size, 10);
    double scale = 1.0;
    if (word_count > 110) scale = 0.6;
    else if (word_count > 80) scale = 0.7;
    else if (word_count > 55) scale = 0.8;
    else if (word_count > 35) scale = 0.9;
    return std::max(10, static_cast<int>(base_size * scale));
}

int adaptive_font_size_translation(const std::string& text, int base_size) {
    int length = static_cast<int>(text.size());
    base_size = std::max(base_size, 10);
    double scale = 1.0;
    if (length > 600) scale = 0.55;
    else if (length > 420) scale = 0.65;
    else if (length > 300) scale = 0.75;
    else if (length > 160) scale = 0.9;
    return std::max(10, static_cast<int>(base_size * scale));
}

bool should_grow(int word_count, const AppConfig& config) {
    return config.enableTextGrowth && word_count < config.textGrowthThreshold;
}

struct FontContext {
    FT_Library ft;
    FT_Face face;
    hb_font_t* hb_font;
};

FontContext init_font(const fs::path& font_file, int font_size) {
    FontContext ctx;
    if (FT_Init_FreeType(&ctx.ft)) throw std::runtime_error("Failed to init FreeType");
    if (FT_New_Face(ctx.ft, font_file.string().c_str(), 0, &ctx.face)) {
        FT_Done_FreeType(ctx.ft);
        throw std::runtime_error("Failed to load font: " + font_file.string());
    }
    FT_Set_Char_Size(ctx.face, 0, font_size * 64, 0, 0);
    ctx.hb_font = hb_ft_font_create(ctx.face, nullptr);
    return ctx;
}

void free_font(FontContext& ctx) {
    hb_font_destroy(ctx.hb_font);
    FT_Done_Face(ctx.face);
    FT_Done_FreeType(ctx.ft);
}

double measure_text_width(FontContext& ctx, const std::string& text) {
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text.c_str(), -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(ctx.hb_font, buf, nullptr, 0);

    unsigned int glyph_count;
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
    double width = 0.0;
    for (unsigned int i = 0; i < glyph_count; ++i) {
        width += glyph_pos[i].x_advance / 64.0;
    }
    hb_buffer_destroy(buf);
    return width;
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
    if (lines.empty()) lines.push_back(text);
    return lines;
}

std::string wrap_single_line(const std::string& line, FontContext& ctx, double max_width, bool isRtl) {
    if (line.empty() || measure_text_width(ctx, line) <= max_width) {
        return isRtl ? LocalizationUtils::reverseWords(line) : line;
    }

    std::istringstream iss(line);
    std::string word;
    std::string current;
    std::vector<std::string> wrapped_lines;
    auto flush_current = [&]() {
        if (!current.empty()) {
            wrapped_lines.push_back(isRtl ? LocalizationUtils::reverseWords(current) : current);
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

    std::string rebuilt;
    for (size_t i = 0; i < wrapped_lines.size(); ++i) {
        rebuilt += wrapped_lines[i];
        if (i + 1 < wrapped_lines.size()) rebuilt += "\\N";
    }
    return rebuilt.empty() ? line : rebuilt;
}

std::string wrap_if_needed(const std::string& text, FontContext& ctx, double max_width, bool isRtl) {
    auto lines = split_ass_lines(text);
    bool applied = false;
    for (auto& line : lines) {
        double width = measure_text_width(ctx, line);
        if (width > max_width) {
            line = wrap_single_line(line, ctx, max_width, isRtl);
            applied = true;
        } else if (isRtl) {
            line = LocalizationUtils::reverseWords(line);
            applied = true;
        }
    }

    if (!applied) return text;
    std::string joined;
    for (size_t i = 0; i < lines.size(); ++i) {
        joined += lines[i];
        if (i + 1 < lines.size()) joined += "\\N";
    }
    return joined;
}

double clamp_padding(double value) {
    return std::clamp(value, 0.0, 0.45);
}

} // namespace

namespace TextLayout {

Engine::Engine(const AppConfig& config)
    : config_(config) {
    paddingPixels_ = config.width * clamp_padding(config.textHorizontalPadding);
    arabicWrapWidth_ = std::max(50.0, (config.width - 2.0 * paddingPixels_) * config.arabicMaxWidthFraction);
    translationWrapWidth_ =
        std::max(50.0, (config.width - 2.0 * paddingPixels_) * config.translationMaxWidthFraction);
}

LayoutResult Engine::layoutVerse(const VerseData& verse) const {
    LayoutResult layout;
    layout.arabicWordCount = count_words(verse.text);
    layout.baseArabicSize = adaptive_font_size_arabic(verse.text, config_.arabicFont.size);
    layout.growArabic = should_grow(layout.arabicWordCount, config_);
    layout.arabicGrowthFactor = layout.growArabic
        ? std::min(config_.maxGrowthFactor, 1.0 + verse.durationInSeconds * config_.growthRateFactor)
        : 1.0;
    int maxArabicSize = std::max(1, static_cast<int>(layout.baseArabicSize * layout.arabicGrowthFactor));

    FontContext arabic_ctx = init_font(config_.arabicFont.file, maxArabicSize);
    layout.wrappedArabic = wrap_if_needed(verse.text, arabic_ctx, arabicWrapWidth_, false);
    free_font(arabic_ctx);

    layout.baseTranslationSize = adaptive_font_size_translation(verse.translation, config_.translationFont.size);
    layout.translationGrowthFactor = layout.growArabic ? layout.arabicGrowthFactor : 1.0;
    int maxTranslationSize =
        std::max(1, static_cast<int>(layout.baseTranslationSize * layout.translationGrowthFactor));

    FontContext translation_ctx = init_font(config_.translationFont.file, maxTranslationSize);
    layout.wrappedTranslation = wrap_if_needed(verse.translation, translation_ctx, translationWrapWidth_, config_.translationIsRtl);
    free_font(translation_ctx);

    return layout;
}

} // namespace TextLayout
