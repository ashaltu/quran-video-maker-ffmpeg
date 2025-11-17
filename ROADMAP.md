# Roadmap & Work in Progress

This document tracks the current development status, known issues, and planned features for quran-video-maker-ffmpeg.

## Project Status

The project is in active development with core functionality working but many features still being refined. Expect bugs and breaking changes as we work toward a stable v1.0 release.

## Testing

Basic unit tests now cover config loading, timing parsing, recitation utilities, and cache helpers. Remaining work is to expand coverage into video generation and add integration/e2e suites, followed by CI automation for regressions and release packaging.

## Core Features (In Progress)

### Multiple Recitation Modes
**Status:** 🟡 Partially Stable

Choose between gapped (ayah-by-ayah with pauses) or gapless (continuous surah) recitation.

**Working:**
- Basic gapped mode rendering
- Basic gapless mode rendering
- Audio caching for both modes
- Mode switching via CLI
- Resilient reciter download/caching shared across gapped and gapless workflows

**TODOs:**
- Investigate and resolve timing precision issues with certain reciter data (e.g., Maher al-Muaiqly showing incorrect segments for Surah Al-Qadr).
- Improve error messages for failed downloads

### Custom Recitations
**Status:** 🔴 Highly Experimental

Use your own audio files with VTT/SRT timing files for complete flexibility.

**Working:**
- Basic VTT/SRT parsing
- Audio download from URLs
- Local audio file support
- Arabic numeral extraction from timing text
- Consistent Bismillah insertion even with custom audio
- Timing alignment survives reciters who rewind or repeat verses
- Parser handles Arabic/non-Arabic numbering within subtitle files
- Custom verse range slicing trims the requested ayat (plus Bismillah) into a standalone clip so late-range renders stay in sync and no longer drop the final verse

**TODOs:**
- Improve VTT/SRT parser robustness and validation
- Add validation of timing file completeness
- Add examples and documentation for creating timing files
- Add CLI flag to disable automatic Bismillah insertion

### Multi-language Support
**Status:** 🟡 Partially Stable

Generate videos with translations in multiple languages.

**Working:**
- English (Sahih International)
- Oromo (Gaali Abba Boor)
- Basic font rendering for each language
- Localized surah labels, names, reciter names, and numerals on intros and thumbnails for languages with assets

**TODOs:**
- Add more translation languages (Urdu, French, Turkish, Indonesian, etc.)
- Implement automatic font selection per language (single font per language)
- Add support for Arabic only, single translation, and two-translations subtitle tracks

### Adaptive Text Sizing
**Status:** 🟡 Partially Stable

Automatically adjusts font sizes based on verse length.

**Working:**
- Basic adaptive sizing for Arabic text
- Basic adaptive sizing for translation text
- Different size thresholds for different verse lengths
- Long verses automatically wrap across multiple lines so they stay on screen
- Growth-aware wrapping now measures text at its final animated size and honors the horizontal/vertical padding knobs so long verses never bleed off screen

**TODOs:**
- Implement size calculations to account for screen aspect ratio changes (current support is for landscape mode)
- Continue tuning adaptive heuristics for portrait canvases and extremely long verses (e.g., 2:282-283) so both Arabic and translations share the available space gracefully

### Text Animations
**Status:** 🟢 Stable

Optional growth animations and fade effects for enhanced visual appeal.

**Working:**
- Text growth animation for shorter verses
- Fade in/out effects
- Configurable animation parameters
- CLI flag to disable animations

**TODOs:**
- Add per-word or per-group of words highlighting synchronized with audio (tricky!)
- Implement adaptive animation timing based on recitation speed (e.g. fades slower for slower-paced reciters)
- Create simple configurable animation presets (e.g. subtle, normal, impact)

### Thumbnail Generation
**Status:** 🟢 Stable

Automatically creates thumbnails with video metadata.

**Working:**
- Thumbnail generation with surah name
- Reciter name display
- Random color selection
- Thumbnail embedded in MP4 metadata

**TODOs:**
- Support custom background image (not just video frame)
- Support using template thumbnails
- Add HD/4K thumbnail support
- Add customizable layouts
- Create separate thumbnail generation command

### Hardware Acceleration
**Status:** 🟡 Platform-Dependent (most likely to be deprecated)

Supports both software and hardware encoders for faster rendering. This feature might be deprecated because there are a lot of complex filters applied during rendering and current benchmarks (at least on a M1 MacBook Pro) show there is no improvement and actually worse performance.

**Working:**
- macOS: VideoToolbox (h264_videotoolbox)
- Software fallback on all platforms (libx264)

**TODOs:**
- Test on Linux with NVIDIA (h264_nvenc)
- Test on Linux with AMD (h264_vaapi)
- Test on Windows platforms

### Intelligent Caching
**Status:** 🟢 Stable

Downloads and caches audio files for reuse across multiple renders.

**Working:**
- Audio file caching in system temp directory
- Reuse of downloaded files
- CLI flag to bypass cache
- CLI flag to clear cache

**TODOs:**
- Implement automatic cache cleanup (currently grows indefinitely)

## Future Features (TODOs)

- Verses:
  - Add single verse previews (should create an image)
- Intelligient Backgrounds(not started):
  - Implement theme selection based on surah content/topic
  - Integrate with [Quranic topics data](https://qul.tarteel.ai/resources/ayah-topics)
  - Support user-uploadable custom backgrounds
  - Add animated backgrounds support
- Simple Video Effects(not started):
  - Add particle effects
  - Implement light rays and glows
  - Create customizable visual themes
- HLS Streaming Support(not started):
  - Generate HLS manifests for progressive playback
  - Support multiple quality renditions (720p, 1080p, 4K)
  - Ensure compatibility with web players and mobile apps
- Full Surah Automation(not started):
  - Implement automatic verse range detection (as opposed to specifying from/to)
  - Add batch rendering for multiple surahs (should be a list or an inclusive range like 110-114)
  - Add progress tracking for long renders

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how you can help move these features forward.

<p align="center">
  <i>May Allah accept this work and make it beneficial. Ameen.</i>
</p>
