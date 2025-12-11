# Changelog

All notable changes to this project will be documented in this file. This project follows [Semantic Versioning](https://semver.org) and takes inspiration from [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] - 2025-10-12

### Added
- **Verse Segmentation for Long Verses**: Break up lengthy verses into timed segments that display sequentially
  - New `--segment-long-verses` flag to enable segmentation for verses defined as "long"
  - New `--segment-data` flag to specify reciter-specific segment timing JSON file
  - New `--long-verses` flag to customize which verses are considered long (default: `metadata/long-verses.json`)
  - Automatic fallback to standard rendering when segment data is unavailable
  - Independent text layout and sizing for each segment
  - Seamless integration with existing gapless/custom audio workflows
- **Long Verses Definition File**: Added `metadata/long-verses.json` containing verse keys for extremely long verses (e.g., 2:282, 58:11)

### Changed
- **Text Layout Engine**: Added `layoutSegment()` method for laying out arbitrary text segments with duration-based growth calculations
- **Subtitle Builder**: Refactored to support mixed verse and segment dialogue generation
  - Segments are timed relative to the video timeline using absolute audio timestamps
  - Each segment receives independent adaptive sizing and wrapping
- **Video Generator**: Updated to accept optional segmentation manager for subtitle generation

### Technical
- **New Module**:
  - `verse_segmentation`: Manager class for loading long verse definitions and reciter segment data, with query methods for determining segmentation eligibility
- **Updated Modules**:
  - `text/text_layout`: Extended Engine class with segment-specific layout method
  - `subtitle_builder`: Restructured dialogue generation to handle both full verses and segments
  - `video_generator`: Plumbed segmentation manager through to subtitle generation

### Segment Data Format
Reciter segment timing files follow this structure:
```json
{
  "19:4": [
    {
      "start": 20.22,
      "end": 24.5,
      "arabic": "قَالَ رَبِّ إِنِّي وَهَنَ ٱلۡعَظۡمُ مِنِّي",
      "translation": "He said, \"My Lord, indeed my bones have weakened",
      "is_last": false
    },
    {
      "start": 24.76,
      "end": 35.0,
      "arabic": "وَٱشۡتَعَلَ ٱلرَّأۡسُ شَيۡبٗا...",
      "translation": "and my head has filled with white...",
      "is_last": true
    }
  ]
}

## [0.2.0] - 2025-12-04

### Added
- **Dynamic Background Videos**: Theme-based background video selection with automatic transitions based on verse ranges
  - Support for R2 cloud storage (public and private buckets)
  - Support for local video directories
  - Configurable theme metadata mapping verses to video themes
  - Deterministic video selection with configurable seed
  - Video standardization utility for preparing background video collections
- **Custom Recitations**: Support for custom audio files with precise verse timing
  - VTT and SRT timing file parsing
  - Automatic Bismillah detection in timing files
  - Support for URL or local file paths
  - Intelligent audio splicing for verse ranges
- **Progress Reporting**: Structured JSON progress events for frontend integration
  - Stage-based progress tracking (subtitles, background, encoding)
  - Real-time encoding progress with percentage, elapsed time, and ETA
  - Parseable output format for UI integration
- **Backend Metadata Generation**: Generate comprehensive metadata JSON for backend servers
  - Complete reciter and translation information
  - Surah names in multiple languages
  - Verse counts and localization data
- **Enhanced Configuration**:
  - `videoSelection` configuration block for dynamic backgrounds
  - `recitationMode` configuration option (gapped/gapless)
  - Environment variable expansion in config files (e.g., `${R2_ENDPOINT}`)
- **New CLI Options**:
  - `--enable-dynamic-bg`: Enable dynamic background video selection
  - `--seed`: Deterministic seed for reproducible results
  - `--local-video-dir`: Use local video directory instead of R2
  - `--r2-endpoint`, `--r2-access-key`, `--r2-secret-key`, `--r2-bucket`: R2 configuration
  - `--custom-audio`, `--custom-timing`: Custom recitation support
  - `--standardize-local`, `--standardize-r2`: Video standardization utilities
  - `--progress`: Emit structured progress events
  - `--generate-backend-metadata`: Generate metadata for backend integration

### Changed
- **Video Pipeline**: Refactored to support dynamic video concatenation without pre-stitching
  - Improved FFmpeg filter complex generation
  - Direct video trimming and concatenation in FFmpeg
  - Better memory efficiency for long video sequences
- **Audio Processing**: Enhanced custom audio processor with intelligent splicing
  - Automatic Bismillah handling for custom recitations
  - Precise timestamp adjustment for verse ranges
  - Support for both gapped and gapless modes
- **Configuration Loading**: Improved path resolution and validation
  - Better asset discovery relative to config file location
  - Automatic fallback to installation directories
  - Enhanced error messages for missing assets
- **Cache System**: Extended caching for background videos
  - Separate cache directory for downloaded R2 videos
  - Cache validation and reuse across renders
- **Metadata Writing**: Enhanced metadata output with complete reproduction information
  - Full command-line argument capture
  - Config file content embedding
  - Absolute path resolution for all assets

### Fixed
- Arabic digit conversion in timing files (e.g., ١٢٣ → 123)
- Bismillah detection in mixed Arabic/English timing files
- Memory leaks in video duration probing
- Path normalization on Windows for FFmpeg arguments
- Font fallback handling for mixed scripts in translations

### Technical
- **New Dependencies**:
  - AWS SDK for C++ (S3 client for R2 integration)
  - Enhanced libav* usage for video duration probing
- **New Modules**:
  - `background_video_manager`: Dynamic background video orchestration
  - `video_selector`: Theme-based video selection with playlist management
  - `r2_client`: R2/S3-compatible object storage client
  - `video_standardizer`: Video normalization utility
  - `timing_parser`: VTT/SRT timing file parser
  - `audio/custom_audio_processor`: Custom audio handling and splicing
- **Refactored Modules**:
  - `video_generator`: Separated concerns for audio, video, and filter generation
  - `config_loader`: Enhanced with environment variable expansion
  - `cache_utils`: Extended for multi-source caching
- **Testing**:
  - Added mock implementations for API and process execution
  - Extended unit test coverage for new modules
  - CI smoke tests for dynamic backgrounds and custom audio

## [0.1.4] - 2025-11-26
### Changed
- Fixed videos not looping.

## [0.1.2 - 0.1.3] - 2025-11-26
### Changed
- Added Windows support.
- Documented CI-tested platforms (Ubuntu 24.04, macOS 15 arm64, Windows Server 2025) and Windows setup expectations in the README.
- Hardened Windows smoke render by normalizing/escaping FFmpeg filter paths so `ass`/`fontsdir` arguments parse correctly.
- Release workflow now reuses CI-produced binary artifacts (no data/assets bundled); data stays external via `data.tar` download.
- Added a Scoop manifest template (`scoop/qvm.json`) for Windows.
- Released 0.1.2 during testing of workflows

## [0.1.1] - 2025-11-25
### Changed
- Replaced Git LFS with a direct download of test data from R2 storage bucket; docs/workflows updated accordingly and zipped data handling removed.
- Enabled built-in RTL handling for subtitles, removing manual reshaping hacks and simplifying text layout code paths.
- Refined config path resolution for clearer override behavior.
- Strengthened CI by adding ffmpeg installation plus a smoke render that checks both the video and generated thumbnail after unit tests.
- Clarified installation instructions in README for manual setups.

## [0.1.0] - 2025-11-22
### Added
- First public release of the `quran-video-maker-ffmpeg` CLI for producing Quran verse videos with synchronized Arabic text, translations, and recitations.
- Gapped rendering pipeline for surah ranges; gapless workflows supported only with user-supplied audio and timing files while built-in gapless data stays disabled.
- Custom recitation inputs via `--custom-audio` and `--custom-timing` (VTT/SRT) with automatic Bismillah insertion and verse-range trimming that keeps late verses aligned.
- Multi-language translations (English, Oromo, Amharic, Urdu) with localized surah labels, reciter names, numerals, and thumbnail/intro text.
- Styling and quality controls in `config.json` plus CLI overrides for fonts, padding, animation toggles, encoders, CRF/bitrate knobs, and named quality profiles.
- Text growth and fade animations with adaptive sizing/wrapping to keep long ayat on-screen.
- Thumbnail generation with randomized color themes and metadata embedding.
- Render metadata sidecars that capture the exact CLI invocation and config artifact, plus structured progress output when `--progress` is enabled.
- Audio download caching with flags to reuse or clear the cache; optional VideoToolbox hardware acceleration on macOS.
- Homebrew tap install (`brew install ashaltu/tap/qvm`) and a starter unit test suite covering config loading, timing parsing, and reciter utilities.

### Known limitations
- Built-in gapless datasets remain disabled pending timing cleanup; gapless renders require providing custom audio and timing files.
- Hardware acceleration is untested on Linux and Windows.
- Build/test coverage: verified on macOS; Linux builds have only light smoke testing; Windows builds and tests have not been done yet.
