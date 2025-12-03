#include "r2_client.h"
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace R2 {

class Client::Impl {
public:
    R2Config config;
    Aws::SDKOptions sdkOptions;
    std::shared_ptr<Aws::S3::S3Client> s3Client;

    explicit Impl(const R2Config& cfg) : config(cfg) {
        Aws::InitAPI(sdkOptions);
        
        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.endpointOverride = extractHost(config.endpoint);
        clientConfig.scheme = Aws::Http::Scheme::HTTPS;
        clientConfig.region = "auto";
        
        if (config.usePublicAccess || config.accessKey.empty() || config.secretKey.empty()) {
            // Public bucket - anonymous credentials
            s3Client = std::make_shared<Aws::S3::S3Client>(
                Aws::Auth::AWSCredentials("", ""),
                clientConfig,
                Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
                false  // useVirtualAddressing
            );
            std::cout << "  Using public R2 bucket access" << std::endl;
        } else {
            // Private bucket - use provided credentials
            s3Client = std::make_shared<Aws::S3::S3Client>(
                Aws::Auth::AWSCredentials(config.accessKey, config.secretKey),
                clientConfig,
                Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
                false  // useVirtualAddressing
            );
            std::cout << "  Using authenticated R2 bucket access" << std::endl;
        }
    }

    ~Impl() {
        Aws::ShutdownAPI(sdkOptions);
    }

private:
    std::string extractHost(const std::string& endpoint) {
        size_t start = endpoint.find("://");
        if (start != std::string::npos) {
            start += 3;
        } else {
            start = 0;
        }
        size_t end = endpoint.find("/", start);
        if (end == std::string::npos) {
            return endpoint.substr(start);
        }
        return endpoint.substr(start, end - start);
    }
};

Client::Client(const R2Config& config)
    : pImpl(std::make_unique<Impl>(config)) {}

Client::~Client() = default;

std::vector<std::string> Client::listVideosInTheme(const std::string& theme) {
    Aws::S3::Model::ListObjectsV2Request request;
    request.SetBucket(pImpl->config.bucket);
    request.SetPrefix(theme + "/");
    
    auto outcome = pImpl->s3Client->ListObjectsV2(request);
    
    if (!outcome.IsSuccess()) {
        auto& error = outcome.GetError();
        throw std::runtime_error(
            "Failed to list videos in theme '" + theme + "': " + 
            error.GetExceptionName() + " - " + error.GetMessage()
        );
    }
    
    std::vector<std::string> videos;
    const auto& objects = outcome.GetResult().GetContents();
    
    for (const auto& object : objects) {
        std::string key = object.GetKey();
        std::string ext = fs::path(key).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || 
            ext == ".mkv" || ext == ".webm") {
            videos.push_back(key);
        }
    }
    
    return videos;
}

std::string Client::downloadVideo(const std::string& key, const fs::path& localPath) {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(pImpl->config.bucket);
    request.SetKey(key);
    
    auto outcome = pImpl->s3Client->GetObject(request);
    
    if (!outcome.IsSuccess()) {
        auto& error = outcome.GetError();
        throw std::runtime_error(
            "Failed to download video '" + key + "': " + 
            error.GetExceptionName() + " - " + error.GetMessage()
        );
    }
    
    fs::create_directories(localPath.parent_path());
    
    std::ofstream outFile(localPath, std::ios::binary);
    if (!outFile.is_open()) {
        throw std::runtime_error("Failed to create output file: " + localPath.string());
    }
    
    auto& body = outcome.GetResult().GetBody();
    outFile << body.rdbuf();
    outFile.close();
    
    if (!fs::exists(localPath) || fs::file_size(localPath) == 0) {
        throw std::runtime_error("Downloaded file is empty or missing: " + localPath.string());
    }
    
    return localPath.string();
}

} // namespace R2