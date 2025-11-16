#include "video_generator.h"
#include "quran_data.h"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <random>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>
#include <future>
#include <cctype>
#include <nlohmann/json.hpp>
#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

extern "C" {
#include <libavformat/avformat.h>
}

namespace fs = std::filesystem;
using json = nlohmann::json;

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

    json load_json_file(const fs::path& path) {
        std::ifstream f(path);
        if (!f.is_open()) return json();
        json data = json::parse(f, nullptr, false);
        if (data.is_discarded()) return json();
        return data;
    }

    std::string get_language_code(const AppConfig& config) {
        return QuranData::getTranslationLanguageCode(config.translationId);
    }

    std::string get_localized_surah_name(int surah, const std::string& lang_code) {
        fs::path path = fs::path("data/surah-names") / (lang_code + ".json");
        json data = load_json_file(path);
        std::string key = std::to_string(surah);
        if (data.is_object()) {
            auto it = data.find(key);
            if (it != data.end() && it->is_string()) {
                return it->get<std::string>();
            }
        }
        return QuranData::surahNames.at(surah);
    }

    std::string get_localized_reciter_name(int reciterId, const std::string& lang_code) {
        fs::path path = fs::path("data/reciter-names") / (lang_code + ".json");
        json data = load_json_file(path);
        std::string key = std::to_string(reciterId);
        if (data.is_object()) {
            auto it = data.find(key);
            if (it != data.end() && it->is_string()) {
                return it->get<std::string>();
            }
        }
        return QuranData::reciterNames.at(reciterId);
    }

    std::string get_localized_surah_label(const std::string& lang_code) {
        json data = load_json_file("data/misc/surah.json");
        if (data.is_object()) {
            auto it = data.find(lang_code);
            if (it != data.end() && it->is_string()) {
                return it->get<std::string>();
            }
            auto fallback = data.find("en");
            if (fallback != data.end() && fallback->is_string()) {
                return fallback->get<std::string>();
            }
        }
        return "Surah";
    }

    std::string get_localized_number(int value, const std::string& lang_code) {
        json data = load_json_file("data/misc/numbers.json");
        auto lookup_for_lang = [&](const std::string& code) -> std::string {
            if (!data.is_object()) return "";
            auto lang_it = data.find(code);
            if (lang_it != data.end() && lang_it->is_object()) {
                std::string key = std::to_string(value);
                auto number_it = lang_it->find(key);
                if (number_it != lang_it->end() && number_it->is_string()) {
                    return number_it->get<std::string>();
                }
            }
            return "";
        };

        std::string localized = lookup_for_lang(lang_code);
        if (localized.empty()) {
            localized = lookup_for_lang("en");
        }
        if (localized.empty()) {
            localized = std::to_string(value);
        }
        return localized;
    }

    bool is_basic_latin_ascii(unsigned char c) {
        return c >= 0x20 && c <= 0x7E;
    }

    std::string apply_latin_font_fallback(const std::string& text,
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

    std::string wrap_if_too_wide_cached(const std::string& text, FontContext& ctx, int video_width, double max_fraction) {
        double width = measure_text_width(ctx, text);
        if (width <= video_width * max_fraction) return text;

        size_t mid = text.size() / 2;
        size_t space_pos = text.rfind(' ', mid);
        if (space_pos == std::string::npos) space_pos = mid;
        std::string wrapped = text;
        wrapped.insert(space_pos, "\\N");
        return wrapped;
    }

    std::string generate_ass_file(const AppConfig& config,
                                  const CLIOptions& options,
                                  const std::vector<VerseData>& verses,
                                  double intro_duration,
                                  double pause_after_intro_duration) {
        fs::path ass_path = fs::temp_directory_path() / "subtitles.ass";
        std::ofstream ass_file(ass_path);
        if (!ass_file.is_open()) throw std::runtime_error("Failed to create temporary subtitle file.");

        std::string language_code = get_language_code(config);
        std::string localized_surah_name = get_localized_surah_name(options.surah, language_code);
        std::string localized_surah_label = get_localized_surah_label(language_code);
        std::string localized_surah_text = localized_surah_label + " " + localized_surah_name;
        std::string localized_surah_text_render =
            apply_latin_font_fallback(localized_surah_text,
                                      QuranData::defaultTranslationFontFamily,
                                      config.translationFont.family);

        ass_file << "[Script Info]\nTitle: Quran Video Subtitles\nScriptType: v4.00+\n";
        ass_file << "PlayResX: " << config.width << "\nPlayResY: " << config.height << "\n\n";

        ass_file << "[V4+ Styles]\n";
        ass_file << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n";
        ass_file << "Style: Arabic," << config.arabicFont.family << "," << config.arabicFont.size << "," << format_ass_color(config.arabicFont.color) << ",&H000000FF,&H00000000,&H99000000,0,0,0,0,100,100,0,0,1,1,1,5,10,10," << config.arabicFont.size * 1.5 << ",1\n";
        ass_file << "Style: Translation," << config.translationFont.family << "," << config.translationFont.size << "," << format_ass_color(config.translationFont.color) << ",&H000000FF,&H00000000,&H99000000,0,0,0,0,100,100,0,0,1,1,1,5,10,10," << config.height / 2 + config.translationFont.size << ",1\n\n";
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
                << ",Translation,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2-1) << ")"
                << "\\fs" << scaled_font_size
                << "\\b1\\bord0\\shad0\\c&HFFFFFF&"
                << "\\fad(0," << config.introFadeOutMs << ")}" 
                << localized_surah_text_render << "\n";

        double cumulative_time = intro_duration + pause_after_intro_duration;
        
        // Process verses in parallel
        std::vector<VerseData> processed_verses(verses.size());
        std::vector<std::future<void>> futures;
        size_t max_threads = std::thread::hardware_concurrency();
        
        for (size_t i = 0; i < verses.size(); ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                processed_verses[i] = verses[i];
                int arabic_size = adaptive_font_size_arabic(processed_verses[i].text, config.arabicFont.size);
                int arabic_word_count = std::count(processed_verses[i].text.begin(), processed_verses[i].text.end(), ' ') + 1;
                
                if (arabic_word_count < config.textWrapThreshold) {
                    FontContext arabic_ctx = init_font(fs::path(config.arabicFont.file), arabic_size);
                    processed_verses[i].text = wrap_if_too_wide_cached(processed_verses[i].text, arabic_ctx, config.width, config.arabicMaxWidthFraction);
                    free_font(arabic_ctx);
                }

                int translation_size = adaptive_font_size_translation(processed_verses[i].translation, config.translationFont.size);
                FontContext translation_ctx = init_font(fs::path(config.translationFont.file), translation_size);
                int translation_word_count = std::count(processed_verses[i].translation.begin(), processed_verses[i].translation.end(), ' ') + 1;
                if (translation_word_count < config.textWrapThreshold) {
                    processed_verses[i].translation = wrap_if_too_wide_cached(processed_verses[i].translation, translation_ctx, config.width, config.translationMaxWidthFraction);
                }
                free_font(translation_ctx);
            }));
            
            if (futures.size() >= max_threads) {
                for (auto& f : futures) f.get();
                futures.clear();
            }
        }
        
        for (auto& f : futures) f.get();

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

            // TODO: smartly fade in/out (e.g. Qari reciting fast means faster fades, otherwise slower fades)
            double fade_time = std::min(std::max(verse.durationInSeconds * config.fadeDurationFactor, config.minFadeDuration), config.maxFadeDuration);
            double growth_factor = (arabic_word_count >= config.textGrowthThreshold) ? 1.0 : std::min(config.maxGrowthFactor, 1.0 + verse.durationInSeconds * config.growthRateFactor);

            std::stringstream combined;
            combined << "{\\an5\\rArabic"
                    << "\\fs" << arabic_size
                    << "\\pos(" << config.width/2 << "," << arabic_y << ")"
                    << "\\fad(" << (fade_time*1000) << "," << (fade_time*1000) << ")";
            if (grow) {
                combined << "\\t(0," << verse.durationInSeconds*1000 << ",\\fs" << arabic_size*growth_factor << ")";
            }
            combined << "}" << verse.text
                    << "\\N{\\an5\\rTranslation"
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
}

void VideoGenerator::generateVideo(const CLIOptions& options, const AppConfig& config, const std::vector<VerseData>& verses) {
    try {
        std::cout << "\n=== Starting Video Rendering ===" << std::endl;
        
        double intro_duration = config.introDuration;
        double pause_after_intro_duration = config.pauseAfterIntroDuration;
        
        std::cout << "Generating subtitles..." << std::endl;
        std::string ass_filename = generate_ass_file(config, options, verses, intro_duration, pause_after_intro_duration);

        double total_duration = intro_duration + pause_after_intro_duration;
        for(const auto& verse : verses) total_duration += verse.durationInSeconds;

        std::stringstream filter_spec;
        filter_spec << "[0:v]loop=loop=-1:size=1:start=0,setpts=N/(FRAME_RATE*TB),scale=" << config.width << ":" << config.height;
        
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
        
        filter_spec << ",ass='" << ass_filename << "':fontsdir='" << fs::absolute(config.assetFolderPath).string() + "/fonts" << "'[v]";

        std::string video_codec;
        if (options.encoder == "hardware") {
            #if defined(__APPLE__)
                video_codec = "-c:v h264_videotoolbox -b:v 3500k -allow_sw 1";
                std::cout << "Using hardware encoder: h264_videotoolbox" << std::endl;
            #else
                video_codec = "-c:v libx264 -preset " + options.preset + " -crf 23";
            #endif
        } else {
            video_codec = "-c:v libx264 -preset " + options.preset + " -crf 23";
            std::cout << "Using software encoder: libx264 ('" << options.preset << "')" << std::endl;
        }

        std::stringstream final_cmd;
        
        // Handle audio differently for gapped vs gapless
        if (config.recitationMode == RecitationMode::GAPLESS) {
            // For gapless: use single surah audio file with precise trimming
            // This is much more efficient than concat - one file read instead of N files
            if (verses.empty()) throw std::runtime_error("No verses to render");
            
            std::string audioPath = verses[0].localAudioPath;
            double startTime = verses[0].timestampFromMs / 1000.0;
            double endTime = verses.back().timestampToMs / 1000.0;
            double audioDuration = endTime - startTime;
            
            // Calculate total video duration
            double totalVideoDuration = intro_duration + pause_after_intro_duration + audioDuration;
            
            // Create silent audio for intro, then append actual recitation
            final_cmd << "ffmpeg -y "
                      << "-stream_loop -1 -i \"" << config.assetBgVideo << "\" "
                      << "-f lavfi -t " << (intro_duration + pause_after_intro_duration) << " -i anullsrc=r=44100:cl=stereo "
                      << "-ss " << startTime << " -t " << audioDuration << " -i \"" << audioPath << "\" "
                      << "-filter_complex \""
                      << "[0:v]setpts=PTS-STARTPTS,scale=" << config.width << ":" << config.height;
            
            if (apply_overlay) {
                final_cmd << ",drawbox=x=0:y=0:w=iw:h=ih:color=" << config.overlayColor << ":t=fill";
            }
            
            final_cmd << ",ass='" << ass_filename << "':fontsdir='" 
                      << fs::absolute(config.assetFolderPath).string() + "/fonts" << "'[v];"
                      << "[1:a][2:a]concat=n=2:v=0:a=1[a]\" "
                      << "-map \"[v]\" -map \"[a]\" "
                      << "-t " << totalVideoDuration << " ";
        } else {
            // For gapped: concatenate individual ayah audio files
            std::string concat_file_path = (fs::temp_directory_path() / "audiolist.txt").string();
            {
                std::ofstream concat_file(concat_file_path);
                if (!concat_file.is_open()) throw std::runtime_error("Failed to create audio list file.");
                for (const auto& verse : verses) {
                    concat_file << "file '" << fs::absolute(verse.localAudioPath).string() << "'\n";
                }
            }
            
            double totalVideoDuration = intro_duration + pause_after_intro_duration;
            for(const auto& verse : verses) totalVideoDuration += verse.durationInSeconds;
            
            final_cmd << "ffmpeg -y "
                      << "-i \"" << config.assetBgVideo << "\" "
                      << "-itsoffset " << (intro_duration + pause_after_intro_duration) << " "
                      << "-f concat -safe 0 -i \"" << concat_file_path << "\" "
                      << "-filter_complex \"" << filter_spec.str() << "\" "
                      << "-map \"[v]\" -map 1:a "
                      << "-t " << totalVideoDuration << " ";
        }

        final_cmd << video_codec << " "
                  << "-c:a aac -b:a 128k "
                  << "-pix_fmt yuv420p "
                  << "-movflags +faststart "
                  << "-threads 8 "
                  << "\"" << options.output << "\"";

        std::cout << "\nExecuting FFmpeg command:\n" << final_cmd.str() << std::endl << std::endl;
        
        int exit_code = system(final_cmd.str().c_str());
        if (exit_code != 0) throw std::runtime_error("FFmpeg execution failed");

        std::cout << "\n✅ Render complete! Video saved to: " << options.output << std::endl;

    } catch(const std::exception& e) {
        std::cerr << "❌ An error occurred during video generation: " << e.what() << std::endl;
    }
}

void VideoGenerator::generateThumbnail(const CLIOptions& options, const AppConfig& config) {
    try {
        std::string output_dir = fs::path(options.output).parent_path().string();
        std::string thumbnail_path = fs::path(output_dir) / "thumbnail.jpeg";

        std::string language_code = get_language_code(config);
        std::string localized_surah_label = get_localized_surah_label(language_code);
        std::string localized_surah_name = get_localized_surah_name(options.surah, language_code);
        std::string localized_reciter_name = get_localized_reciter_name(config.reciterId, language_code);
        std::string localized_surah_number = get_localized_number(options.surah, language_code);

        auto with_fallback = [&](const std::string& text) {
            return apply_latin_font_fallback(text,
                                             QuranData::defaultTranslationFontFamily,
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
                << "," << pick_color() << ",&H000000FF&, &H003333&, &H00000000&,1,0,0,0,100,100,0,0,1,3,1,3,10,10,10,1\n";
        ass_file << "Style: Main," << config.translationFont.family << "," << scaled_font_size 
                << "," << pick_color() << ",&H000000FF&, &H000000&, &H00000000&,1,0,0,0,100,100,0,0,1,5,3,5,10,10,10,1\n";
        ass_file << "Style: Reciter," << config.translationFont.family << "," << reciter_size 
                << "," << pick_color() << ",&H000000FF&, &H003333&, &H00000000&,1,0,0,0,100,100,0,0,1,3,1,3,10,10,10,1\n";
        ass_file << "Style: Number," << config.translationFont.family << "," << number_size 
                << "," << number_color << ",&H000000FF&, &H003333&, &H00000000&,1,0,0,0,100,100,0,0,1,5,3,5,10,10,10,1\n\n";

        ass_file << "[Events]\n";
        ass_file << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Label,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2 - scaled_font_size*0.6) << ")\\fad(0," << config.introFadeOutMs << ")}" << rendered_label << "\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Main,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2) << ")\\fad(0," << config.introFadeOutMs << ")}" << rendered_surah_name << "\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Reciter,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << (config.height/2 + scaled_font_size*0.6) << ")\\fad(0," << config.introFadeOutMs << ")}" << rendered_reciter_name << "\n";
        ass_file << "Dialogue: 0,0:00:00.00,0:00:05.00,Number,,0,0,0,,{\\an" << align << "\\pos(" << number_x << ",50)\\fad(0," << config.introFadeOutMs << ")}" << rendered_surah_number << "\n";
                
        ass_file.close();

        std::string fonts_dir = fs::absolute(config.assetFolderPath).string() + "/fonts";

        std::stringstream cmd;
        cmd << "ffmpeg -y "
            << "-ss 0 "
            << "-i \"" << config.assetBgVideo << "\" "
            << "-vf \"ass='" << ass_path.string() << "':fontsdir='" << fonts_dir << "'\" "
            << "-frames:v 1 "
            << "-q:v 2 "
            << "\"" << thumbnail_path << "\"";

        int exit_code = system(cmd.str().c_str());
        if (exit_code != 0) throw std::runtime_error("FFmpeg thumbnail generation failed");

        std::cout << "✅ Thumbnail saved to: " << thumbnail_path << std::endl;

    } catch(const std::exception& e) {
        std::cerr << "❌ An error occurred during thumbnail generation: " << e.what() << std::endl;
    }
}
