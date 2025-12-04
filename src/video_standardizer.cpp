#include "video_standardizer.h"
#include "r2_client.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>

extern "C" {
#include <libavformat/avformat.h>
}

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace VideoStandardizer {

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// clean me  up by removing boolean flag and splitting into two functions
void standardizeDirectory(const std::string& path, bool isR2Bucket) {
    if (isR2Bucket) {
        standardizeR2Bucket(path);
        return;
    }
    
    if (!fs::exists(path)) {
        throw std::runtime_error("Directory does not exist: " + path);
    }
    
    std::cout << "Standardizing videos in: " << path << std::endl;
    
    json metadata;
    metadata["standardizedAt"] = getCurrentTimestamp();
    metadata["videos"] = json::array();
    
    int totalVideos = 0;
    double totalDuration = 0.0;
    
    // Process each theme directory
    for (const auto& themeEntry : fs::directory_iterator(path)) {
        if (!themeEntry.is_directory()) continue;
        
        std::string theme = themeEntry.path().filename().string();
        std::cout << "\nProcessing theme: " << theme << std::endl;
        
        for (const auto& videoEntry : fs::directory_iterator(themeEntry)) {
            if (!videoEntry.is_regular_file()) continue;
            
            std::string ext = videoEntry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext != ".mp4" && ext != ".mov" && ext != ".avi" && 
                ext != ".mkv" && ext != ".webm") continue;
            
            // Skip already standardized files
            auto stem = videoEntry.path().stem().string();
            // (s.size() >= 4 && s.compare(s.size() - 4, 4, "_std") == 0)
            if (stem.size() >= 4 && stem.compare(stem.size() - 4, 4, "_std") == 0){
                std::cout << "  Already standardized: " << videoEntry.path().filename() << std::endl;
                continue;
            }
            
            // Standardize the video
            fs::path outputPath = videoEntry.path().parent_path() / 
                                  (videoEntry.path().stem().string() + "_std.mp4");
            
            std::ostringstream cmd;
            cmd << "ffmpeg -y -i \"" << videoEntry.path().string() << "\" "
                << "-c:v libx264 -preset fast -crf 23 "
                << "-s 1280x720 -r 30 "
                << "-pix_fmt yuv420p "
                << "-an "  // Remove audio
                << "-movflags +faststart "
                << "\"" << outputPath.string() << "\" 2>/dev/null";
            
            std::cout << "  Standardizing: " << videoEntry.path().filename() << " -> " 
                      << outputPath.filename() << std::endl;
            
            int result = std::system(cmd.str().c_str());
            if (result == 0 && fs::exists(outputPath)) {
                // Get duration
                AVFormatContext* ctx = nullptr;
                double duration = 0.0;
                if (avformat_open_input(&ctx, outputPath.string().c_str(), nullptr, nullptr) == 0) {
                    if (avformat_find_stream_info(ctx, nullptr) >= 0) {
                        duration = static_cast<double>(ctx->duration) / AV_TIME_BASE;
                    }
                    avformat_close_input(&ctx);
                }
                
                // Remove original
                fs::remove(videoEntry.path());
                
                // Add to metadata
                json videoInfo;
                videoInfo["theme"] = theme;
                videoInfo["filename"] = outputPath.filename().string();
                videoInfo["duration"] = duration;
                metadata["videos"].push_back(videoInfo);
                
                totalVideos++;
                totalDuration += duration;
            } else {
                std::cerr << "  Failed to standardize: " << videoEntry.path().filename() << std::endl;
            }
        }
    }
    
    metadata["totalVideos"] = totalVideos;
    metadata["totalDuration"] = totalDuration;
    
    // Save metadata
    fs::path metadataPath = fs::path(path) / "metadata.json";
    std::ofstream metaFile(metadataPath);
    metaFile << metadata.dump(2);
    
    std::cout << "\n✅ Standardization complete!" << std::endl;
    std::cout << "Total videos: " << totalVideos << std::endl;
    std::cout << "Total duration: " << totalDuration << " seconds" << std::endl;
    std::cout << "Metadata saved to: " << metadataPath << std::endl;
}

void standardizeR2Bucket(const std::string& bucketName) {
    std::cout << "Standardizing R2 bucket: " << bucketName << std::endl;
    
    // Get R2 config from environment
    R2::R2Config r2Config;
    r2Config.bucket = bucketName;
    r2Config.endpoint = std::getenv("R2_ENDPOINT") ? std::getenv("R2_ENDPOINT") : "";
    r2Config.accessKey = std::getenv("R2_ACCESS_KEY") ? std::getenv("R2_ACCESS_KEY") : "";
    r2Config.secretKey = std::getenv("R2_SECRET_KEY") ? std::getenv("R2_SECRET_KEY") : "";
    r2Config.usePublicAccess = false;
    
    if (r2Config.endpoint.empty() || r2Config.accessKey.empty() || r2Config.secretKey.empty()) {
        throw std::runtime_error("R2 credentials not set. Please set R2_ENDPOINT, R2_ACCESS_KEY, and R2_SECRET_KEY environment variables.");
    }
    
    R2::Client r2Client(r2Config);
    
    // Create temp directory for processing
    fs::path tempDir = fs::temp_directory_path() / ("r2_standardize_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(tempDir);
    
    json metadata;
    metadata["bucket"] = bucketName;
    metadata["standardizedAt"] = getCurrentTimestamp();
    metadata["videos"] = json::array();
    
    int totalVideos = 0;
    double totalDuration = 0.0;
    
    try {
        // List all themes
        auto themes = r2Client.listThemes();
        
        for (const auto& theme : themes) {
            std::cout << "\nProcessing theme: " << theme << std::endl;
            
            // List videos in theme
            auto videos = r2Client.listVideosInTheme(theme);
            
            for (const auto& videoKey : videos) {
                std::string filename = fs::path(videoKey).filename().string();
                
                // Skip already standardized files
                if (filename.find("_std.mp4") != std::string::npos) {
                    std::cout << "  Already standardized: " << filename << std::endl;
                    continue;
                }
                
                // Download video
                fs::path localPath = tempDir / filename;
                std::cout << "  Downloading: " << filename << std::endl;
                
                try {
                    r2Client.downloadVideo(videoKey, localPath);
                } catch (const std::exception& e) {
                    std::cerr << "  Download failed: " << e.what() << std::endl;
                    continue;
                }
                
                // Standardize the video
                std::string stdFilename = fs::path(filename).stem().string() + "_std.mp4";
                fs::path stdPath = tempDir / stdFilename;
                
                std::ostringstream cmd;
                cmd << "ffmpeg -y -i \"" << localPath.string() << "\" "
                    << "-c:v libx264 -preset fast -crf 23 "
                    << "-s 1280x720 -r 30 "
                    << "-pix_fmt yuv420p "
                    << "-an "  // Remove audio
                    << "-movflags +faststart "
                    << "\"" << stdPath.string() << "\" 2>/dev/null";
                
                std::cout << "  Standardizing: " << filename << " -> " << stdFilename << std::endl;
                
                int result = std::system(cmd.str().c_str());
                if (result == 0 && fs::exists(stdPath)) {
                    // Get duration
                    AVFormatContext* ctx = nullptr;
                    double duration = 0.0;
                    if (avformat_open_input(&ctx, stdPath.string().c_str(), nullptr, nullptr) == 0) {
                        if (avformat_find_stream_info(ctx, nullptr) >= 0) {
                            duration = static_cast<double>(ctx->duration) / AV_TIME_BASE;
                        }
                        avformat_close_input(&ctx);
                    }
                    
                    // Upload standardized video
                    std::string newKey = theme + "/" + stdFilename;
                    std::cout << "  Uploading: " << newKey << std::endl;
                    
                    if (r2Client.uploadVideo(stdPath, newKey)) {
                        // Delete original from R2
                        r2Client.deleteObject(videoKey);
                        
                        // Add to metadata
                        json videoInfo;
                        videoInfo["theme"] = theme;
                        videoInfo["filename"] = stdFilename;
                        videoInfo["key"] = newKey;
                        videoInfo["duration"] = duration;
                        metadata["videos"].push_back(videoInfo);
                        
                        totalVideos++;
                        totalDuration += duration;
                    }
                    
                    // Clean up local files
                    fs::remove(localPath);
                    fs::remove(stdPath);
                } else {
                    std::cerr << "  Failed to standardize: " << filename << std::endl;
                }
            }
        }
        
        metadata["totalVideos"] = totalVideos;
        metadata["totalDuration"] = totalDuration;
        
        // Upload metadata to R2
        fs::path metadataPath = tempDir / "metadata.json";
        std::ofstream metaFile(metadataPath);
        metaFile << metadata.dump(2);
        metaFile.close();
        
        r2Client.uploadVideo(metadataPath, "metadata.json");
        
        std::cout << "\n✅ R2 bucket standardization complete!" << std::endl;
        std::cout << "Total videos: " << totalVideos << std::endl;
        std::cout << "Total duration: " << totalDuration << " seconds" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during R2 standardization: " << e.what() << std::endl;
    }
    
    // Clean up temp directory
    fs::remove_all(tempDir);
}

} // namespace VideoStandardizer