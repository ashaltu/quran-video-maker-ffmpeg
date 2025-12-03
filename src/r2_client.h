#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace R2 {

struct R2Config {
    std::string endpoint;
    std::string accessKey;
    std::string secretKey;
    std::string bucket;
};

class Client {
public:
    explicit Client(const R2Config& config);
    ~Client();

    // List all video files in a theme directory
    std::vector<std::string> listVideosInTheme(const std::string& theme);
    
    // Download video to local path
    std::string downloadVideo(const std::string& key, const std::filesystem::path& localPath);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace R2