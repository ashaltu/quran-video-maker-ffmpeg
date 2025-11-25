# Changelog

All notable changes to this project will be documented in this file. This project follows [Semantic Versioning](https://semver.org) and takes inspiration from [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]
### Changed
- Documented CI-tested platforms (Ubuntu 24.04, macOS 15 arm64, Windows Server 2025) and Windows setup expectations in the README.
- Hardened Windows smoke render by normalizing/escaping FFmpeg filter paths so `ass`/`fontsdir` arguments parse correctly.
- Release workflow now reuses CI-produced binary artifacts (no data/assets bundled); data stays external via `data.tar` download.
- Added a Scoop manifest template (`scoop/qvm.json`) for Windows; bucket maintainers must set the `hash` for each release.

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
