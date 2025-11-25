# Contributing to quran-video-maker-ffmpeg

First off, JazakumAllahu Khayr for considering contributing to this project! This tool is built to serve everyone including Muslims, non-Muslims, Arabic readers, and non-Arabic readers by helping them seek tawbah and draw closer to Allah.

## Our Mission

This project exists to make Quranic content accessible and engaging for everyone. We aim to create beautiful videos that engage eyes, ears, and hearts simultaneously, facilitating sincere reflection and understanding of the Quran.

## Guiding Principles

When contributing, please keep these principles in mind:

- **Sincerity**: All work should be done seeking Allah's pleasure
- **Humbleness**: We're all learning and growing together
- **Educational Focus**: Features should facilitate learning and reflection
- **Distraction-Free**: Avoid adding features that could lead to mindless consumption
- **Shirk-Free**: No content or features that could compromise tawheed
- **Accessibility**: Make the tool usable for people of all backgrounds and technical levels
- **Quality**: The Quran deserves our best effort in presentation and accuracy

## How Can I Contribute?

### Reporting Bugs

Before creating a bug report, please check existing issues to avoid duplicates. When you create a bug report, include as many details as possible:

**Bug Report Template:**
```markdown
**Description:**
A clear description of the bug

**To Reproduce:**
Steps to reproduce the behavior:
1. Run command '...'
2. With parameters '...'
3. See error

**Expected Behavior:**
What you expected to happen

**Environment:**
- OS: [e.g., Ubuntu 22.04, macOS 14.0]
- FFmpeg version: [e.g., 6.0]
- Compiler: [e.g., GCC 11.4, Clang 15.0]

**Additional Context:**
Any other context, screenshots, or sample files
```

### Suggesting Enhancements

We welcome suggestions for new features or improvements! Please:

1. Check if the enhancement has already been suggested
2. Provide a clear use case for the feature
3. Explain how it aligns with the project's mission
4. Consider the technical feasibility

**Enhancement Template:**
```markdown
**Feature Description:**
Clear description of the proposed feature

**Use Case:**
Who would benefit and how?

**Implementation Ideas:**
Any thoughts on how this could be implemented?

**Alignment with Mission:**
How does this help users engage with the Quran?
```

### Contributing Code

#### Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/your-username/quran-video-maker-ffmpeg.git`
3. Download and extract test data: `curl -L https://qvm-r2-storage.tawbah.app/data.tar -o data.tar && tar -xf data.tar`
4. Create a branch: `git checkout -b feature/your-feature-name`
5. Make your changes
6. Test thoroughly
7. Commit with clear messages
8. Push to your fork
9. Open a Pull Request

#### Code Style

- Follow existing code style and conventions
- Use meaningful variable and function names
- Comment complex logic
- Keep functions focused and manageable in size
- Prefer clarity over cleverness

#### Commit Messages

Write clear, descriptive commit messages:

```
Good: Fix audio/text sync issue in gapless mode
Bad: fix bug

Good: Add support for Urdu translation with Nastaliq font
Bad: update translation stuff
```

#### Pull Request Process

1. **Update Documentation**: If you've added features, update the README or relevant docs
2. **Test Your Changes**: Ensure all existing functionality still works
3. **Describe Your Changes**: Clearly explain what and why in the PR description
4. **Link Issues**: Reference any related issues
5. **Be Patient**: Reviews may take time, especially during busy periods
6. **Be Receptive**: Be open to feedback and suggestions

**Pull Request Template:**
```markdown
**Description:**
What does this PR do?

**Related Issues:**
Fixes #123, Relates to #456

**Testing:**
How did you test these changes?

**Screenshots/Videos:**
If applicable, add visual proof of changes

**Checklist:**
- [ ] Code follows project style guidelines
- [ ] Documentation updated
- [ ] Tested on multiple surahs/reciters
- [ ] No breaking changes (or documented if unavoidable)
```

### Priority Areas

We especially welcome contributions in these areas:

1. **Bug Fixes**: Addressing known issues listed in README
2. **Performance**: Optimizing rendering speed and memory usage
3. **Reciter/Translation Support**: Adding new reciters and translations
4. **Platform Support**: Testing and fixes for different OS/architectures
5. **Documentation**: Improving guides, examples, and comments
6. **Testing**: Writing tests and test cases
7. **Timing Accuracy**: Improving verse timing precision
8. **Font Support**: Adding support for more languages and scripts

## Adding a New Language or Translation

Follow these steps whenever you add support for another translation/language:

1. **Pick a QUL Translation**: Download the JSON file for your language from the [Quranic Universal Library translation list](https://qul.tarteel.ai/resources/translation) and place it under `data/translations/<lang-code>/`. Use the same schema as the existing files (keyed by verse like `"2:255"` with `{"t": "..."}`).
2. **Register It in Code**: Update `src/quran_data.h`:
   - Add the translation to `translationFiles` with a new numeric `translationId`.
   - Map the `translationId` to its language code (e.g., `en`, `om`, `amh`) in `translationLanguages`.
   - Point `translationFontMappings` and `translationFontFamilies` to an appropriate font (add the font to `assets/fonts/` if needed).
3. **Localized Labels & Numbers**: Edit `data/misc/surah.json` to add the localized word for “Surah”, and add a full 1–114 mapping for your language inside `data/misc/numbers.json`.
4. **Transliterated Names**: Create/update the following files (mirroring the structure of the existing ones):
   - `data/surah-names/<lang-code>.json` – localized/transliterated surah names.
   - `data/reciter-names/<lang-code>.json` – localized/transliterated reciter names (keys must match the IDs in `src/quran_data.h`).
5. **Smoke Test**: Run the generator with `--translation <your-id>` to confirm the translation is picked up, the intro/thumbnail shows localized text, and fonts render correctly.

If you miss any of the steps above, the intro card or thumbnail can fall back to English strings even though the subtitles are localized, so please double-check each file before opening a PR.

### Areas Needing Extra Care

These areas require careful consideration:

- **Audio Processing**: Changes here can affect sync with text
- **Text Rendering**: Must preserve Arabic script integrity
- **FFmpeg Integration**: Performance-critical, test thoroughly
- **Timing Parsers**: Must handle edge cases correctly
- **Configuration**: Breaking changes should be avoided

## Development Setup

### Prerequisites

Ensure you have all dependencies installed as per the [Installation Guide](README.md#installation).

### Building for Development

```bash
# Clean build recommended for major changes
rm -rf build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Testing Your Changes


#### Testing `qvm` executable works locally
```bash
rm -rf build-test
cmake -S . -B build-test -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PWD/build-test/install
cmake --build build-test --target install

export QVM_PREFIX=$PWD/build-test/install
export PATH="$QVM_PREFIX/bin:$PATH"

cd /tmp # or any other folder just not in the repo directory so we're sure no cheating happens because we are in the root directory that has all the assets/data

qvm 1 1 7 # creates surah fatiha. you can test with any other config values or cli flags 
```

#### Testing Homebrew packaging (when touching config paths/assets/data)
- Update `qvm.rb` in your tap (`$(brew --repo ashaltu/homebrew-tap)/Formula/qvm.rb`), then reinstall from source:
  ```bash
  brew reinstall --build-from-source ashaltu/tap/qvm
  qvm 1 1 7  # Should run without missing data/assets
  ```
- Verify data landed under `$(brew --prefix)/share/quran-video-maker/data`:
  ```bash
  ls $(brew --prefix)/share/quran-video-maker/data/quran/qpc-hafs-word-by-word.json
  ```
- Optional: run a local smoke render from within the repo similar to CI (must've built already):
  ```bash
  ./build/qvm 1 1 7 --translation 1 --output out/local-smoke.mp4
  test -s out/local-smoke.mp4
  test -s out/thumbnail.jpeg
  ```

Always test with various surahs:
- Short surahs (e.g., Al-Fatiha)
- Long surahs (e.g., Al-Baqarah)
- Different reciters and modes
- Edge cases (Surah 9 without Bismillah)

### Debugging

Use verbose FFmpeg output for debugging:
```bash
FFREPORT=file=ffmpeg.log:level=32 ./build/quran-video-maker ...
```

## Code of Conduct

### Our Standards

- Be respectful and considerate
- Welcome newcomers and help them learn
- Focus on what's best for the community
- Show empathy and kindness
- Give and receive constructive feedback gracefully

### Unacceptable Behavior

- Disrespectful or inflammatory comments
- Personal attacks or insults
- Trolling or inappropriate jokes
- Publishing others' private information
- Any behavior that violates Islamic adab (etiquette)

### Enforcement

Maintainers have the right to remove comments, commits, or contributions that don't align with these guidelines. In serious cases, contributors may be temporarily or permanently banned.

## Questions?

Don't hesitate to ask questions by:
- Opening an issue labeled "question"
- Starting a discussion in GitHub Discussions (if enabled)

## Recognition

Contributors will be acknowledged in the project. More importantly, we hope this work is accepted by Allah as a means of drawing closer to Him.

## License

By contributing, you agree that your contributions will be licensed under the GNU General Public License v3.0.

<p align="center">
  <i>May Allah accept our efforts and make this work beneficial for the Ummah. Ameen.</i>
</p>
