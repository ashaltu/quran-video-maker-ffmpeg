# quran-video-maker-ffmpeg [WIP]

<p align="center">
  A high-performance C++/FFmpeg tool for generating beautiful Quran verse videos with synchronized Arabic text, translations, and recitations.
</p>

<p align="center">
  <strong>⚠️ This project is in active development. A lightweight unit test suite is included, but expect rapid changes.</strong>
</p>


## Overview

**quran-video-maker-ffmpeg** creates professionally styled Quran videos by combining:
- High-quality audio recitations from world-renowned reciters
- Arabic text in Uthmanic script with word-by-word data
- Translations in multiple languages
- Localized surah titles, reciter names, and numerals in the translation language for intros and thumbnails
- Dynamic animations including text growth and fade effects
- Customizable backgrounds and visual themes

The tool supports both gapped (ayah-by-ayah) and gapless (continuous) workflows. Custom recitations can be supplied via `--custom-audio` and `--custom-timing`.

## Installing

### With Homebrew (macOS/Linux)

```bash
brew install ashaltu/tap/qvm

qvm 1 1 7 # Generates video for the entire Surah Fatiha
```

### With Scoop (Windows)

```powershell
scoop install https://github.com/ashaltu/quran-video-maker-ffmpeg/releases/latest/download/scoop-qvm.json

qvm 1 1 7
```

Data is fetched automatically via the Scoop manifest.

### Manually

#### Prerequisites

- **C++ Compiler**: Supporting C++17 or later
- **CMake**: Version 3.16 or higher
- **FFmpeg Libraries**: libavformat, libavcodec, libavfilter, libavutil, libswscale
- **FreeType2**: For font rendering
- **HarfBuzz**: For text shaping (especially important for Arabic)
- **System Libraries**: PkgConfig, Threads

#### 1. Clone the Repository

```bash
git clone https://github.com/ashaltu/quran-video-maker-ffmpeg.git
cd quran-video-maker-ffmpeg
```

#### 2. Fetch and Extract Test Data

Download the prepackaged test data archive:

```bash
curl -L https://qvm-r2-storage.tawbah.app/data.tar -o data.tar
tar -xf data.tar

# You may remove the original archive if you like:
# rm data.tar
```

This contains a subset of Quranic data (audio, translations, scripts) needed for local development and testing and unpacks directly into `data/`. For production use or additional resources, visit the [QUL Resources page](https://qul.tarteel.ai/resources/).
Releases do not bundle data; always download `data.tar` from the R2 link above (same for WSL).

#### 3. Install System Dependencies

**macOS (tested locally + GitHub Actions macos-15-arm64, 15.7.1 / 24G231):**
```bash
brew install cmake pkg-config ffmpeg freetype harfbuzz
```

**Ubuntu/Debian (tested locally + GitHub Actions ubuntu-24.04):**
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  libavformat-dev libavcodec-dev libavfilter-dev \
  libavutil-dev libswscale-dev libswresample-dev \
  libfreetype6-dev libharfbuzz-dev
```

**Windows (tested on GitHub Actions windows-2025 runner):**
- Manual (MSYS2 UCRT64):
  ```bash
  pacman -S --noconfirm \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-ffmpeg \
    mingw-w64-ucrt-x86_64-freetype \
    mingw-w64-ucrt-x86_64-harfbuzz \
    mingw-w64-ucrt-x86_64-curl \
    mingw-w64-ucrt-x86_64-cpr \
    mingw-w64-ucrt-x86_64-nlohmann-json \
    mingw-w64-ucrt-x86_64-cxxopts
  ```
  Then build with CMake/Ninja under MSYS2, download `data.tar` to the repo root, `tar -xf data.tar`, and run `./build/qvm.exe ...`.

Scoop (Windows):
```powershell
scoop install https://github.com/ashaltu/quran-video-maker-ffmpeg/releases/latest/download/scoop-qvm.json
qvm 1 1 7
```

#### 4. Build the Project

```bash
mkdir build && cd build
cmake ..
cmake --build .
cd ..
```

The executable will be available at `./build/quran-video-maker`.

#### 5. Run Unit Tests

```bash
cd build
ctest --output-on-failure
cd ..
```

The tests exercise config loading, timing parsing, recitation utilities, subtitle generation helpers, the text layout engine, and the custom audio splicer plan builder. Please run them before submitting changes.

#### 6. Create video

Please check the the `/out` folder after these commands are run.

Generate a video for Surah Al-Fatiha (verses 1-7):

```bash
./build/quran-video-maker 1 1 7 \
  --reciter 2 \
  --translation 1
```

> Gapless mode notice: built-in gapless datasets are still disabled. Use `--custom-audio` + `--custom-timing` to run gapless/custom recitations.

## Configuration

The tool uses a `config.json` file for default settings. You can override these via command-line options.

Key rendering knobs inside `config.json` include `textHorizontalPadding` (fractional left/right padding reserved for both Arabic and translation lines), `textVerticalPadding` (top/bottom guard rails that keep subtitles from touching the screen edge), `arabicMaxWidthFraction`, and `translationMaxWidthFraction`. Together they control how aggressively long verses wrap before reaching the screen edge and how much breathing room you get when growth animations are enabled. If you use a translation font that cannot render ASCII digits or Latin characters cleanly, set `translationFallbackFontFamily` to the font the renderer should temporarily swap to for those glyph ranges.

The `qualityProfile` block governs encoder defaults (preset, CRF, pixel format, bitrate). Three built-in profiles are available:

- `speed` – ultrafast preview renders with higher CRF.
- `balanced` – default “fast” preset with moderate CRF (~21) and 4.5Mbps target bitrate.
- `max` – slow preset, CRF ~18, 10-bit output, and higher bitrates suitable for archival uploads.

You can override any individual quality parameter via CLI (`--quality-profile`, `--crf`, `--pix-fmt`, `--video-bitrate`, `--maxrate`, `--bufsize`).

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--reciter, -r` | Reciter ID (see src/quran_data.h) | From config |
| `--translation, -t` | Translation ID | From config |
| `--mode, -m` | Recitation mode: `gapped` (active) or `gapless` (temporarily disabled pending data cleanup) | `gapped` |
| `--output, -o` | Output filename | `out/surah-X_Y-Z.mp4` |
| `--width` | Video width | 1280 |
| `--height` | Video height | 720 |
| `--fps` | Frames per second | 30 |
| `--arabic-font-size` | Override Arabic subtitle font size (px) | From config (default 100) |
| `--translation-font-size` | Override translation subtitle font size (px) | From config (default 50) |
| `--encoder, -e` | Encoder: `software` or `hardware` | `software` |
| `--preset, -p` | Software encoder preset for speed/quality | `fast` |
| `--quality-profile` | Quality profile: `speed`, `balanced`, `max` | `balanced` |
| `--crf` | Force CRF value (0–51). Lower = higher quality | From profile/config |
| `--pix-fmt` | Pixel format (e.g. `yuv420p10le`) | From profile/config |
| `--video-bitrate` | Target video bitrate (e.g. `6000k`) | From profile/config |
| `--maxrate` | Maximum encoder bitrate (e.g. `8000k`) | From profile/config |
| `--bufsize` | Encoder buffer size (e.g. `12000k`) | From profile/config |
| `--no-cache` | Disable caching | false |
| `--clear-cache` | Clear all cached data | false |
| `--no-growth` | Disable text growth animations | false |
| `--progress` | Emit `PROGRESS {...}` logs for machine-readable status | false |
| `--custom-audio` | Custom audio file path or URL (gapless only) | - |
| `--custom-timing` | Custom timing file (VTT or SRT, required with custom audio) | - |
| `--text-padding` | Override horizontal padding fraction (0-0.45) applied to both languages | From config (default 0.05) |

**Note:** When using custom audio & timing, the renderer trims the requested range, inserts a Bismillah clip, and re-bases the verse timings so your clip can start at any ayah. Built-in gapless data is still disabled for now, so gapless renders require `--custom-audio` + `--custom-timing`.

#### Quality Profiles

`config.json` now exposes a `qualityProfiles` object where you can describe presets for `speed`, `balanced`, `max`, or any custom label you invent. Each entry can override `preset`, `crf`, `pixelFormat`, and the optional bitrate knobs. The CLI flag `--quality-profile` simply picks one of those blocks (default: `balanced`) and still allows overriding individual values via `--preset`, `--crf`, `--pix-fmt`, `--video-bitrate`, `--maxrate`, and `--bufsize`.

### Render Metadata Sidecar

Every render writes a JSON sidecar next to the video (e.g., `out/surah-1_1-7.metadata.json`). It captures:

- The exact CLI invocation (`argv`, shell-quoted string, binary path, working directory)
- Absolute paths for outputs, config, assets, and any custom audio/timing files
- A copy of the config file contents plus size/modified timestamp for reproducibility

Use it as an audit trail for automation pipelines or to compare settings across runs. New CLI/config knobs automatically show up in the metadata because the writer preserves the raw config artifact.

### Progress Monitoring

Pass `--progress` to emit deterministic log lines that start with `PROGRESS ` followed by JSON:

```
PROGRESS {"stage":"encoding","status":"running","percent":37.50,"elapsedSeconds":12.4,"etaSeconds":20.6,"message":"Encoding in progress"}
```

The default behavior remains unchanged; you only see these structured lines when `--progress` is supplied. They’re designed for job runners (Express workers, queues, etc.) to parse and forward to clients. Additional stages (e.g., subtitle generation) also announce when they start/finish.

### Localization Assets

The renderer keeps the intro cards and thumbnails in sync with the chosen translation language. Language-specific resources are stored in the `data` folder:

- `data/misc/surah.json` – localized label for the word “Surah”
- `data/misc/numbers.json` – numerals for surah numbers (1–114) per language code
- `data/surah-names/<lang>.json` – transliterated or localized surah names
- `data/reciter-names/<lang>.json` – transliterated reciter names

Whenever localized metadata falls back to Basic Latin characters (e.g., a missing translation for “Surah” or digits you kept as `0-9`), the renderer automatically switches those characters to the default translation font (`American Captain`) so you get a predictable look instead of OS fallback fonts.

Each translation ID is associated with a language code in `src/quran_data.h`. When adding a translation, make sure the code has entries in all of the files above and that the translation JSON (following the [QUL format](https://qul.tarteel.ai/resources/translation)) lives under `data/translations/<lang>/`. See [CONTRIBUTING.md](CONTRIBUTING.md) for a full checklist.

## Performance

### Benchmarks

Rendering times on a MacBook Pro M1 (with `--preset ultrafast`):

| Surah | Verses | Mode | Time |
|-------|--------|------|------|
| Al-Fatiha (1) | 1-7 | Gapped | ~5s |
| Al-Mu'minun (23) | 1-118 | Gapless | ~2m |
| Al-Baqarah (2) | 1-286 | Gapless | ~22m |

### Optimizations

- Parallel Processing: Text measurements and wrapping computed in parallel
- Efficient Audio Handling: Gapless mode uses optimized audio concatenation
- Smart Caching: Downloaded audio and metadata cached for reuse
- Hardware Acceleration: Optional hardware encoder support (macOS: VideoToolbox)

## Data Sources & Credits

This project relies on high-quality Quranic data from:

- [Quranic Universal Library (QUL)](https://qul.tarteel.ai) - Tarteel.ai's comprehensive Quranic database providing translations, audio, and Uthmanic script data
- [QuranicAudio.com](https://quranicaudio.com) - High-quality recitation audio files from world-renowned reciters
- [Quran.com](https://quran.com) - Reference for verse data and translations
- [Quran Caption](https://qurancaption.com) - For easily creating SRT/VTT files

Special thanks to these organizations for making their resources freely available to serve the Muslim community.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

See [CONTRIBUTING.md](CONTRIBUTING.md) for how you can help move [ROADMAP.md](ROADMAP.md) features forward.

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

See the [LICENSE](LICENSE) file for details.

<p align="center">
  <i>May Allah accept this work and make it a means of drawing closer to Him. Ameen.</i>
</p>
