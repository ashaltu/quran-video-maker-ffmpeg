#pragma once
#include <string>

namespace VideoStandardizer {
    void standardizeDirectory(const std::string& path, bool isR2Bucket = false);
    void standardizeR2Bucket(const std::string& bucketName);
    std::string getCurrentTimestamp();
}