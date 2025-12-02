#include "metadata_writer.h"
#include "quran_data.h"
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <ctime>
#include <iostream>
#include <map>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

fs::path getExecutablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    return fs::path(buffer).parent_path();
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        return fs::path(buffer).parent_path();
    }
    return fs::path();
#endif
}

std::string getFullTranslationName(const std::string& filename) {
    std::string name = filename.substr(0, filename.find("-clean.json"));
    std::replace(name.begin(), name.end(), '-', ' ');
    name[0] = toupper(name[0]);
    for (size_t i = 1; i < name.length(); ++i) {
        if (name[i - 1] == ' ') {
            name[i] = toupper(name[i]);
        }
    }
    return name;
}

std::string iso8601Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) {
        return "";
    }
    return buffer;
}

std::string fileTimeToIso(const fs::file_time_type& fileTime) {
    try {
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        const auto seconds = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm{};
#if defined(_WIN32)
        gmtime_s(&tm, &seconds);
#else
        gmtime_r(&seconds, &tm);
#endif
        char buffer[32];
        if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) {
            return "";
        }
        return buffer;
    } catch (...) {
        return "";
    }
}

bool isLikelyUri(const std::string& value) {
    auto pos = value.find("://");
    return pos != std::string::npos && pos > 0;
}

std::string safeAbsolutePath(const fs::path& path) {
    if (path.empty()) return "";
    std::error_code ec;
    fs::path absolutePath = fs::absolute(path, ec);
    if (ec) return path.string();
    return absolutePath.lexically_normal().string();
}

std::string safeAbsolutePath(const std::string& value) {
    if (value.empty() || isLikelyUri(value)) return value;
    return safeAbsolutePath(fs::path(value));
}

std::string safeCurrentPath() {
    std::error_code ec;
    auto cwd = fs::current_path(ec);
    if (ec) return "";
    return cwd.lexically_normal().string();
}

std::string joinArgsForShell(const std::vector<std::string>& args) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& arg : args) {
        if (!first) oss << ' ';
        first = false;
        oss << std::quoted(arg);
    }
    return oss.str();
}

json buildCommandBlock(const std::vector<std::string>& rawArgs) {
    json command;
    command["argv"] = rawArgs;
    command["joined"] = joinArgsForShell(rawArgs);
    if (!rawArgs.empty()) {
        command["binary"] = safeAbsolutePath(rawArgs[0]);
    }
    command["workingDirectory"] = safeCurrentPath();
    return command;
}

json buildPathsBlock(const CLIOptions& options,
                     const AppConfig& config,
                     const fs::path& metadataPath) {
    json paths;
    paths["metadata"] = safeAbsolutePath(metadataPath);
    paths["output"] = safeAbsolutePath(options.output);
    paths["config"] = safeAbsolutePath(options.configPath);
    paths["assets"] = safeAbsolutePath(config.assetFolderPath);
    paths["backgroundVideo"] = safeAbsolutePath(config.assetBgVideo);
    paths["quranWordByWordData"] = safeAbsolutePath(config.quranWordByWordPath);
    if (!options.customAudioPath.empty() && !isLikelyUri(options.customAudioPath)) {
        paths["customAudio"] = safeAbsolutePath(options.customAudioPath);
    }
    if (!options.customTimingFile.empty() && !isLikelyUri(options.customTimingFile)) {
        paths["customTiming"] = safeAbsolutePath(options.customTimingFile);
    }
    return paths;
}

std::string readFileContents(const fs::path& path, std::error_code& ecOut) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ecOut = std::error_code(errno, std::generic_category());
        return "";
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

json buildConfigArtifact(const std::string& configPath) {
    json artifact;
    fs::path path = configPath.empty() ? fs::path("./config.json") : fs::path(configPath);
    artifact["path"] = safeAbsolutePath(path);

    std::error_code ec;
    bool exists = fs::exists(path, ec);
    artifact["exists"] = exists && !ec;
    if (ec) {
        artifact["error"] = ec.message();
        return artifact;
    }
    if (!exists) {
        artifact["error"] = "Config file not found";
        return artifact;
    }

    std::error_code sizeEc;
    auto size = fs::file_size(path, sizeEc);
    if (!sizeEc) {
        artifact["sizeBytes"] = size;
    }

    std::error_code timeEc;
    auto modified = fs::last_write_time(path, timeEc);
    if (!timeEc) {
        artifact["modifiedAt"] = fileTimeToIso(modified);
    }

    std::error_code readEc;
    auto contents = readFileContents(path, readEc);
    if (readEc) {
        artifact["contentReadError"] = readEc.message();
    } else {
        artifact["content"] = contents;
    }
    return artifact;
}

json buildArtifactsBlock(const CLIOptions& options) {
    json artifacts;
    artifacts["config"] = buildConfigArtifact(options.configPath);
    return artifacts;
}

} // namespace

namespace MetadataWriter {

void writeMetadata(const CLIOptions& options,
                   const AppConfig& config,
                   const std::vector<std::string>& rawArgs) {
    fs::path outputPath = options.output.empty() ? fs::path("out/render.mp4") : fs::path(options.output);
    fs::path metadataPath = outputPath;
    metadataPath.replace_extension(".metadata.json");

    fs::path parentDir = metadataPath.parent_path();
    if (!parentDir.empty() && !fs::exists(parentDir)) {
        fs::create_directories(parentDir);
    }

    json metadata;
    metadata["generatedAt"] = iso8601Timestamp();
    metadata["command"] = buildCommandBlock(rawArgs);
    metadata["paths"] = buildPathsBlock(options, config, metadataPath);
    metadata["artifacts"] = buildArtifactsBlock(options);

    std::ofstream file(metadataPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to write metadata file: " + metadataPath.string());
    }
    file << metadata.dump(2) << '\n';
}

void generateBackendMetadata(std::ostream& out) {
    fs::path dataPath = getExecutablePath() / "data";
    json metadata;

    // Reciters
    json reciters = json::array();
    for (const auto& pair : QuranData::reciterNames) {
        reciters.push_back({
            {"id", pair.first},
            {"name", pair.second}
        });
    }
    metadata["reciters"] = reciters;

    // Translations
    json translations = json::array();
    for (const auto& translationFile : QuranData::translationFiles) {
        fs::path path(translationFile.second);
        translations.push_back({
            {"id", translationFile.first},
            {"name", getFullTranslationName(path.filename().string())}
        });
    }
    metadata["translations"] = translations;

    // Surahs
    json surahs = json::object();
    std::ifstream arSurahNamesFile(dataPath / "surah-names/ar.json");
    json arSurahNamesData;
    if (arSurahNamesFile.is_open()) {
        arSurahNamesFile >> arSurahNamesData;
    }

    for (int i = 1; i <= 114; ++i) {
        surahs[std::to_string(i)] = {
            {"en_name", QuranData::surahNames.at(i)},
            {"ar_name", arSurahNamesData.count(std::to_string(i)) ? arSurahNamesData[std::to_string(i)] : ""},
            {"verse_count", QuranData::verseCounts.at(i)}
        };
    }
    metadata["surahs"] = surahs;

    // Misc
    json misc = json::object();
    std::ifstream surahFile(dataPath / "misc/surah.json");
    if (surahFile.is_open()) {
        json surahData;
        surahFile >> surahData;
        misc["surah"] = surahData;
    }
    std::ifstream numbersFile(dataPath / "misc/numbers.json");
    if (numbersFile.is_open()) {
        json numbersData;
        numbersFile >> numbersData;
        misc["numbers"] = numbersData;
    }
    metadata["misc"] = misc;

    out << metadata.dump() << std::endl;
}


} // namespace MetadataWriter
