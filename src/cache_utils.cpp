#include "cache_utils.h"
#include "quran_data.h"
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <cctype>
#include <stdexcept>
#include <system_error>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <iostream>
#include <cpr/cpr.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
    fs::path initialDataRoot() {
        const char* envDataRoot = std::getenv("QVM_DATA_ROOT");
        if (envDataRoot && *envDataRoot) {
            return fs::path(envDataRoot);
        }
        return fs::current_path();
    }

    fs::path dataRoot = initialDataRoot();

    fs::path determineDefaultCacheRoot() {
        const char* envCache = std::getenv("QVM_CACHE_DIR");
        if (envCache && *envCache) {
            return fs::path(envCache);
        }

#ifdef _WIN32
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData && *localAppData) {
            return fs::path(localAppData) / "quran-video-maker";
        }
        const char* appData = std::getenv("APPDATA");
        if (appData && *appData) {
            return fs::path(appData) / "quran-video-maker";
        }
#elif __APPLE__
        const char* home = std::getenv("HOME");
        if (home && *home) {
            return fs::path(home) / "Library" / "Caches" / "quran-video-maker";
        }
#else
        const char* xdg = std::getenv("XDG_CACHE_HOME");
        if (xdg && *xdg) {
            return fs::path(xdg) / "quran-video-maker";
        }
        const char* home = std::getenv("HOME");
        if (home && *home) {
            return fs::path(home) / ".cache" / "quran-video-maker";
        }
#endif
        return fs::temp_directory_path() / "quran-video-maker-cache";
    }

    fs::path cacheRoot = determineDefaultCacheRoot();

    std::mutex translationCacheMutex;
    std::unordered_map<int, json> translationCache;
    std::mutex reciterCacheMutex;
    std::unordered_map<int, json> reciterAudioCache;

    void ensure_parent(const fs::path& path) {
        const auto parent = path.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            fs::create_directories(parent, ec);
        }
    }
}

void CacheUtils::setDataRoot(const fs::path& root) {
    if (root.empty()) return;
    std::error_code ec;
    fs::path resolved = fs::canonical(root, ec);
    if (ec) {
        resolved = fs::absolute(root, ec);
    }
    if (!resolved.empty()) {
        dataRoot = resolved;
    }
}

fs::path CacheUtils::getDataRoot() {
    return dataRoot;
}

fs::path CacheUtils::resolveDataPath(const fs::path& relativePath) {
    fs::path path(relativePath);
    if (path.is_absolute()) {
        return path;
    }
    return dataRoot / path;
}

void CacheUtils::setCacheRoot(const fs::path& root) {
    if (root.empty()) return;
    std::error_code ec;
    fs::path resolved = fs::absolute(root, ec);
    cacheRoot = ec ? root : resolved;
}

fs::path CacheUtils::getCacheRoot() {
    return cacheRoot;
}

const json& CacheUtils::getTranslationData(int translationId) {
    std::lock_guard<std::mutex> lock(translationCacheMutex);
    auto it = translationCache.find(translationId);
    if (it == translationCache.end()) {
        auto fileIt = QuranData::translationFiles.find(translationId);
        if (fileIt == QuranData::translationFiles.end()) {
            throw std::runtime_error("Unknown translationId: " + std::to_string(translationId));
        }
        fs::path translationPath = resolveDataPath(fileIt->second);
        std::ifstream file(translationPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open translation file: " + translationPath.string());
        }
        json data = json::parse(file, nullptr, true, true);
        it = translationCache.emplace(translationId, std::move(data)).first;
    }
    return it->second;
}

std::string CacheUtils::getTranslationText(int translationId, const std::string& verseKey) {
    const json& translations = getTranslationData(translationId);
    auto it = translations.find(verseKey);
    if (it != translations.end() && it->is_object()) {
        auto textIt = it->find("t");
        if (textIt != it->end() && textIt->is_string()) {
            return textIt->get<std::string>();
        }
    }
    return "";
}

const json& CacheUtils::getReciterAudioData(int reciterId) {
    std::lock_guard<std::mutex> lock(reciterCacheMutex);
    auto it = reciterAudioCache.find(reciterId);
    if (it == reciterAudioCache.end()) {
        auto recIt = QuranData::reciterFiles.find(reciterId);
        if (recIt == QuranData::reciterFiles.end()) {
            throw std::runtime_error("Unknown reciterId for gapped mode: " + std::to_string(reciterId));
        }
        fs::path metadataPath = resolveDataPath(recIt->second);
        std::ifstream file(metadataPath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open reciter metadata file: " + metadataPath.string());
        }
        json data = json::parse(file, nullptr, true, true);
        it = reciterAudioCache.emplace(reciterId, std::move(data)).first;
    }
    return it->second;
}

fs::path CacheUtils::buildCachedAudioPath(const std::string& label) {
    std::error_code ec;
    fs::path audioDir = cacheRoot / "audio";
    fs::create_directories(audioDir, ec);
    return audioDir / label;
}

bool CacheUtils::fileIsValid(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::file_size(path, ec) > 0;
}

std::string CacheUtils::sanitizeLabel(std::string value) {
    for (char& ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
            ch = '_';
        }
    }
    return value;
}

bool CacheUtils::downloadFileWithRetry(const std::string& url, const fs::path& destination, int maxRetries) {
    ensure_parent(destination);
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            throw std::runtime_error("Unable to open destination for download: " + destination.string());
        }

        cpr::Session session;
        session.SetUrl(cpr::Url{url});
        session.SetTimeout(cpr::Timeout{60000});
        session.SetVerifySsl(cpr::VerifySsl{true});
        session.SetRedirect(cpr::Redirect{true});
        session.SetHeader(cpr::Header{{"User-Agent", "quran-video-maker/1.0"}});
        auto response = session.Download(out);
        out.close();

        const bool ok = response.error.code == cpr::ErrorCode::OK &&
                        response.status_code >= 200 && response.status_code < 400 &&
                        fileIsValid(destination);
        if (ok) {
            return true;
        }

        if (attempt == maxRetries) {
            std::cerr << "  ! Download failed for " << url
                      << " (HTTP " << response.status_code
                      << ", cpr error=" << static_cast<int>(response.error.code)
                      << " - " << response.error.message << ")" << std::endl;
        }

        std::error_code ec;
        fs::remove(destination, ec);
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * attempt));
    }
    return false;
}
