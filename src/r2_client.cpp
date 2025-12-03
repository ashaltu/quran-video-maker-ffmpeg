#include "r2_client.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }
    return escaped.str();
}

std::string sha256Hash(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string hmacSha256(const std::string& key, const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    HMAC(EVP_sha256(), key.c_str(), key.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         hash, nullptr);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string getAmzDate() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    gmtime_r(&time_t, &tm);
    char buffer[17];
    std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &tm);
    return buffer;
}

std::string getDateStamp() {
    return getAmzDate().substr(0, 8);
}

} // namespace

namespace R2 {

class Client::Impl {
public:
    R2Config config;

    explicit Impl(const R2Config& cfg) : config(cfg) {}

    cpr::Header generateAuthHeaders(const std::string& method,
                                    const std::string& path,
                                    const std::string& queryString = "") {
        std::string amzDate = getAmzDate();
        std::string dateStamp = getDateStamp();
        
        std::string canonicalUri = path;
        std::string canonicalQueryString = queryString;
        std::string canonicalHeaders = 
            "host:" + extractHost(config.endpoint) + "\n" +
            "x-amz-content-sha256:UNSIGNED-PAYLOAD\n" +
            "x-amz-date:" + amzDate + "\n";
        std::string signedHeaders = "host;x-amz-content-sha256;x-amz-date";
        std::string payloadHash = "UNSIGNED-PAYLOAD";
        
        std::string canonicalRequest = 
            method + "\n" +
            canonicalUri + "\n" +
            canonicalQueryString + "\n" +
            canonicalHeaders + "\n" +
            signedHeaders + "\n" +
            payloadHash;
        
        std::string algorithm = "AWS4-HMAC-SHA256";
        std::string credentialScope = dateStamp + "/auto/s3/aws4_request";
        std::string stringToSign = 
            algorithm + "\n" +
            amzDate + "\n" +
            credentialScope + "\n" +
            sha256Hash(canonicalRequest);
        
        std::string kDate = hmacSha256("AWS4" + config.secretKey, dateStamp);
        std::string kRegion = hmacSha256(kDate, "auto");
        std::string kService = hmacSha256(kRegion, "s3");
        std::string kSigning = hmacSha256(kService, "aws4_request");
        std::string signature = hmacSha256(kSigning, stringToSign);
        
        std::string authorizationHeader = 
            algorithm + " " +
            "Credential=" + config.accessKey + "/" + credentialScope + ", " +
            "SignedHeaders=" + signedHeaders + ", " +
            "Signature=" + signature;
        
        return cpr::Header{
            {"Authorization", authorizationHeader},
            {"x-amz-date", amzDate},
            {"x-amz-content-sha256", payloadHash},
            {"Host", extractHost(config.endpoint)}
        };
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
    std::string path = "/" + pImpl->config.bucket + "/";
    std::string prefix = theme + "/";
    std::string queryString = "list-type=2&prefix=" + urlEncode(prefix);
    
    auto headers = pImpl->generateAuthHeaders("GET", path, queryString);
    std::string url = pImpl->config.endpoint + path + "?" + queryString;
    
    auto response = cpr::Get(
        cpr::Url{url},
        headers,
        cpr::VerifySsl{true}
    );
    
    if (response.status_code != 200) {
        throw std::runtime_error("Failed to list videos in theme '" + theme + 
                               "': HTTP " + std::to_string(response.status_code));
    }
    
    // Parse XML response (simplified - you may want to use a proper XML parser)
    std::vector<std::string> videos;
    std::string content = response.text;
    size_t pos = 0;
    
    while ((pos = content.find("<Key>", pos)) != std::string::npos) {
        pos += 5;
        size_t end = content.find("</Key>", pos);
        if (end == std::string::npos) break;
        
        std::string key = content.substr(pos, end - pos);
        std::string ext = fs::path(key).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || 
            ext == ".mkv" || ext == ".webm") {
            videos.push_back(key);
        }
        pos = end;
    }
    
    return videos;
}

std::string Client::downloadVideo(const std::string& key, const fs::path& localPath) {
    std::string path = "/" + pImpl->config.bucket + "/" + key;
    auto headers = pImpl->generateAuthHeaders("GET", path);
    std::string url = pImpl->config.endpoint + path;
    
    fs::create_directories(localPath.parent_path());
    std::ofstream out(localPath, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open output file: " + localPath.string());
    }
    
    auto response = cpr::Download(
        out,
        cpr::Url{url},
        headers,
        cpr::VerifySsl{true}
    );
    
    out.close();
    
    if (response.status_code != 200) {
        fs::remove(localPath);
        throw std::runtime_error("Failed to download video '" + key + 
                               "': HTTP " + std::to_string(response.status_code));
    }
    
    return localPath.string();
}

} // namespace R2