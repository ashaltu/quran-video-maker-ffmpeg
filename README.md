# quran-video-maker-ffmpeg [WIP]

<p align="center">
  A high-performance C++/FFmpeg tool for generating beautiful Quran verse videos with synchronized Arabic text, translations, and recitations.
</p>

<p align="center">
  <strong>⚠️ This project is in active development. Testing has not been setup. Expect bugs and breaking changes.</strong>
</p>


## Overview

**quran-video-maker-ffmpeg** creates professionally styled Quran videos by combining:
- High-quality audio recitations from world-renowned reciters
- Arabic text in Uthmanic script with word-by-word data
- Translations in multiple languages
- Localized surah titles, reciter names, and numerals in the translation language for intros and thumbnails
- Dynamic animations including text growth and fade effects
- Customizable backgrounds and visual themes

The tool currently only supports gapped mode (ayah-by-ayah with pauses). Gapless mode is temporarily disabled because it's too buggy and the gapless data needs to be cleaned first.

## Prerequisites

- **C++ Compiler**: Supporting C++17 or later
- **CMake**: Version 3.16 or higher
- **FFmpeg Libraries**: libavformat, libavcodec, libavfilter, libavutil, libswscale
- **FreeType2**: For font rendering
- **HarfBuzz**: For text shaping (especially important for Arabic)
- **System Libraries**: PkgConfig, Threads

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/ashaltu/quran-video-maker-ffmpeg.git
cd quran-video-maker-ffmpeg
```

### 2. Extract Test Data

Extract the mini data archive for testing:

```bash
tar -xvf data.tar
unzip 'data/*.zip' -d data
rm data/*.zip

# The original data.tar remains, but you can delete that too with `rm data.tar`
```

This contains a subset of Quranic data (audio, translations, scripts) needed for local development and testing. For production use or additional resources, visit the [QUL Resources page](https://qul.tarteel.ai/resources/).

### 3. Install System Dependencies

**macOS:**
```bash
brew install cmake pkg-config ffmpeg freetype harfbuzz
```

**Ubuntu/Debian(UNTESTED):**
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  libavformat-dev libavcodec-dev libavfilter-dev \
  libavutil-dev libswscale-dev \
  libfreetype6-dev libharfbuzz-dev
```

### 4. Build the Project

```bash
mkdir build && cd build
cmake ..
cmake --build .
cd ..
```

The executable will be available at `./build/quran-video-maker`.

## Quick Start

Please check the the `/out` folder after these commands are run.

Generate a video for Surah Al-Fatiha (verses 1-7):

```bash
./build/quran-video-maker 1 1 7 \
  --reciter 2 \
  --translation 1
```

> Gapless mode notice: `--mode gapless` (and custom recitation options) currently exit immediately with `Error: Gapless mode is temporarily disabled because it's too buggy and the gapless data needs to be cleaned first.` Once the gapless dataset is cleaned, this workflow will return.

## Configuration

The tool uses a `config.json` file for default settings. You can override these via command-line options.

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
| `--preset, -p` | Encoder preset: `ultrafast`, `fast`, `medium` | `fast` |
| `--no-cache` | Disable caching | false |
| `--clear-cache` | Clear all cached data | false |
| `--no-growth` | Disable text growth animations | false |
| `--custom-audio` | Custom audio file path or URL (gapless only, currently disabled) | - |
| `--custom-timing` | Custom timing file (VTT or SRT, currently disabled) | - |

**Note:** Gapless mode and all custom recitation options are temporarily disabled while the dataset is cleaned; running with those flags exits with `Error: Gapless mode is temporarily disabled because it's too buggy and the gapless data needs to be cleaned first.`

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
