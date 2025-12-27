#include "subtitle_builder.h"
#include "quran_data.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
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

// Helper struct for segment dialogue generation
struct SegmentDialogue {
    double startTime;
    double endTime;
    std::string arabicText;
    std::string translationText;
    int arabicSize;
    int translationSize;
    double arabicGrowthFactor;
    double translationGrowthFactor;
    bool growEnabled;
};

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
                         double pause_after_intro_duration,
                         const VerseSegmentation::Manager* segmentManager) {
    fs::path ass_path = fs::temp_directory_path() / "subtitles.ass";
    std::ofstream ass_file(ass_path);
    if (!ass_file.is_open()) throw std::runtime_error("Failed to create temporary subtitle file.");

    std::string language_code = LocalizationUtils::getLanguageCode(config);
    std::string localized_surah_name = LocalizationUtils::getLocalizedSurahName(options.surah, language_code);
    std::string localized_surah_label = LocalizationUtils::getLocalizedSurahLabel(language_code);
    std::string localized_surah_text = localized_surah_label + " " + localized_surah_name;

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
    ass_file << "Style: Arabic," << config.arabicFont.family << "," << config.arabicFont.size << "," << format_ass_color(config.arabicFont.color) << ",&H000000FF,&H00000000,&H99000000,0,0,0,0,100,100,0,0,1,1,1,5," << styleMargin << "," << styleMargin << "," << config.arabicFont.size * 1.5 << ",-1\n";
    ass_file << "Style: Translation," << config.translationFont.family << "," << config.translationFont.size << "," << format_ass_color(config.translationFont.color) << ",&H000000FF,&H00000000,&H99000000,0,0,0,0,100,100,0,0,1,1,1,5," << styleMargin << "," << styleMargin << "," << config.height / 2 + config.translationFont.size << ",-1\n\n";
    ass_file << "[Events]\n";
    ass_file << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

    // Intro subtitle
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
    
    range_text = applyLatinFontFallback(range_text,
                                        config.translationFallbackFontFamily,
                                        config.translationFont.family);

    ass_file << "Dialogue: 0,0:00:00.00," << format_time_ass(intro_duration)
            << ",Translation,,0,0,0,,{\\an5\\pos(" << config.width/2 << "," << config.height/2 + scaled_font_size*1.5 << ")"
            << "\\fs" << scaled_font_size/2
            << "\\b0\\bord2\\shad1\\be1\\c&HFFFFFF&\\3c&H000000&"
            << "\\fad(0," << config.introFadeOutMs << ")}"
            << range_text << "\n";

    // Collect all dialogue entries (verses and segments)
    std::vector<SegmentDialogue> allDialogues;
	
	//***//
	// Calculate total video duration (needed for potential header)  
	double total_video_duration = intro_duration + pause_after_intro_duration;  
	for (const auto& verse : verses) {  
		total_video_duration += verse.durationInSeconds;  
	}  
	  
	// Persistent header with surah name - shows throughout entire video if enabled  
	if (options.showSurahHeader) {  
		// Use custom font size and margin from CLI options  
		int header_font_size = options.surahHeaderFontSize;  
		int header_y_position = options.surahHeaderMarginTop;  
		  
		// Prefix with Arabic "سورہٴ" before the localized surah name  
		std::string header_text = "سورہٴ " + localized_surah_name;  
		  
		// Use Arabic font for the header (no font fallback needed)  
		std::string header_text_render = "{\\fn" + config.surahHeaderFont.family + "}" + header_text;  
	  
		// Start AFTER intro duration to avoid duplicate display  
		double header_start_time = intro_duration + pause_after_intro_duration;  
	  
		ass_file << "Dialogue: 0," << format_time_ass(header_start_time) << ","  
				<< format_time_ass(total_video_duration)  
				<< ",Translation,,0,0,0,,{\\an8\\pos(" << config.width/2 << "," << header_y_position << ")"  
				<< "\\fs" << header_font_size  
				<< "\\b0\\bord2\\shad1\\be1\\c&HFFFFFF&\\3c&H000000&}"  
				//<< "\\alpha&H80&}" // Semi-transparent 
				<< header_text_render << "\n";  
	}
    
    double cumulative_time = intro_duration + pause_after_intro_duration;
    double verticalPadding = config.height * std::clamp(config.textVerticalPadding, 0.0, 0.3);

    for (size_t idx = 0; idx < verses.size(); ++idx) {
        const VerseData& verse = verses[idx];

        double verse_audio_start = verse.timestampFromMs / 1000.0;
        
        // Check if this verse should be segmented
        bool useSegmentation = segmentManager && 
                               segmentManager->isEnabled() && 
                               segmentManager->shouldSegmentVerse(verse.verseKey);
        
        if (useSegmentation) {
            // Get segments for this verse
            auto segments = segmentManager->getSegments(verse.verseKey);
            
            std::cout << "  Segmenting verse " << verse.verseKey << " into " 
                      << segments.size() << " parts" << std::endl;
            
            for (size_t segIdx = 0; segIdx < segments.size(); ++segIdx) {
                const auto& segment = segments[segIdx];
                
                // Calculate segment timing relative to video timeline
                // segment.startSeconds is absolute time in the audio file
                // verse_audio_start is when this verse starts in the audio
                // cumulative_time is when this verse starts in the video
                double segment_offset_from_verse = segment.startSeconds - verse_audio_start;
                double segment_start_in_video = cumulative_time + segment_offset_from_verse;
                double segment_end_in_video = cumulative_time + (segment.endSeconds - verse_audio_start);
                double segment_duration = segment.endSeconds - segment.startSeconds;
                
                // Layout the segment text
                auto layout = layoutEngine.layoutSegment(segment.arabic, 
                                                          segment.translation, 
                                                          segment_duration);
                
                SegmentDialogue dialogue;
                dialogue.startTime = segment_start_in_video;
                dialogue.endTime = segment_end_in_video;
                dialogue.arabicText = layout.wrappedArabic;
                dialogue.translationText = applyLatinFontFallback(
                    layout.wrappedTranslation, 
                    config.translationFallbackFontFamily, 
                    config.translationFont.family);
                dialogue.arabicSize = layout.baseArabicSize;
                dialogue.translationSize = layout.baseTranslationSize;
                dialogue.arabicGrowthFactor = layout.arabicGrowthFactor;
                dialogue.translationGrowthFactor = layout.translationGrowthFactor;
                dialogue.growEnabled = layout.growArabic;
                
                allDialogues.push_back(dialogue);
            }
        } else {
            // Standard verse handling (no segmentation)
            auto layout = layoutEngine.layoutVerse(verse);
            
            SegmentDialogue dialogue;

			// Extract verse number from verseKey and convert to Arabic digits directly  
			size_t colon_pos = verse.verseKey.find(':');  
			if (colon_pos != std::string::npos) {  
				std::string raw_verse_number = verse.verseKey.substr(colon_pos + 1);  
				  
				// Only append verse number to first ayat when skip-start-bismillah is enabled  
				if (idx == 0 && options.skipStartBismillah) {   
					// Convert each digit to Arabic equivalent  
					std::string arabic_digits[] = {"٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"};  
					std::string localized_verse_number = "";  
					  
					for (char c : raw_verse_number) {  
						if (c >= '0' && c <= '9') {  
							localized_verse_number += arabic_digits[c - '0'];  
						}  
					}  
					  
					dialogue.arabicText = layout.wrappedArabic + " " + localized_verse_number;  
				} else {  
					dialogue.arabicText = layout.wrappedArabic;  
				}  
			} else {  
				dialogue.arabicText = layout.wrappedArabic;  
			}
					  
            dialogue.startTime = cumulative_time;
            dialogue.endTime = cumulative_time + verse.durationInSeconds;
            dialogue.translationText = applyLatinFontFallback(
                layout.wrappedTranslation, 
                config.translationFallbackFontFamily, 
                config.translationFont.family);
            dialogue.arabicSize = layout.baseArabicSize;
            dialogue.translationSize = layout.baseTranslationSize;
            dialogue.arabicGrowthFactor = layout.arabicGrowthFactor;
            dialogue.translationGrowthFactor = layout.translationGrowthFactor;
            dialogue.growEnabled = layout.growArabic;
			
            
            allDialogues.push_back(dialogue);
        }
        
        cumulative_time += verse.durationInSeconds;
    }

    // Generate dialogue lines for all entries
    for (const auto& dialogue : allDialogues) {
        int arabic_size = dialogue.arabicSize;
        int translation_size = dialogue.translationSize;
        double duration = dialogue.endTime - dialogue.startTime;

        // Scale down if needed to fit screen
        double max_total_height = config.height * 0.8;
        double estimated_height = arabic_size * 1.2 + translation_size * 1.4;
        if (estimated_height > max_total_height) {
            double scale_factor = max_total_height / estimated_height;
            arabic_size = static_cast<int>(arabic_size * scale_factor);
            translation_size = static_cast<int>(translation_size * scale_factor);
        }

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
            std::max(duration * config.fadeDurationFactor, config.minFadeDuration),
            config.maxFadeDuration);

        std::stringstream combined;
        combined << "{\\an5\\q2\\rArabic"
                 << "\\fs" << arabic_size
                 << "\\pos(" << config.width / 2 << "," << arabic_y << ")"
                 << "\\fad(" << (fade_time * 1000) << "," << (fade_time * 1000) << ")";
        if (dialogue.growEnabled) {
            combined << "\\t(0," << duration * 1000 << ",\\fs" 
                     << arabic_size * dialogue.arabicGrowthFactor << ")";
        }
        combined << "}" << dialogue.arabicText
                 << "\\N{\\an5\\q2\\rTranslation"
                 << "\\fs" << translation_size
                 << "\\pos(" << config.width / 2 << "," << translation_y << ")"
                 << "\\fad(" << (fade_time * 1000) << "," << (fade_time * 1000) << ")";
        if (dialogue.translationGrowthFactor > 1.0) {
            combined << "\\t(0," << duration * 1000 << ",\\fs"
                     << translation_size * dialogue.translationGrowthFactor << ")";
        }
        combined << "}" << dialogue.translationText;

        ass_file << "Dialogue: 0," << format_time_ass(dialogue.startTime) << ","
                 << format_time_ass(dialogue.endTime)
                 << ",Translation,,0,0,0,," << combined.str() << "\n";
    }

    return ass_path.string();
}

} // namespace SubtitleBuilder