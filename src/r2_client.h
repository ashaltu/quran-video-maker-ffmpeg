#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <memory>

namespace R2 {

struct R2Config {
    std::string endpoint;
    std::string accessKey;
    std::string secretKey;
    std::string bucket;
    bool usePublicAccess = true;
};

class Client {
public:
    explicit Client(const R2Config& config);
    ~Client();

    // List all video files in a theme directory
    std::vector<std::string> listVideosInTheme(const std::string& theme);
    
    // List all themes (directories) in bucket
    std::vector<std::string> listThemes();
    
    // Download video to local path
    std::string downloadVideo(const std::string& key, const std::filesystem::path& localPath);
    
    // Upload video from local path
    bool uploadVideo(const std::filesystem::path& localPath, const std::string& key);
    
    // Delete object from bucket
    bool deleteObject(const std::string& key);
    
    // Check if object exists
    bool objectExists(const std::string& key);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace R2