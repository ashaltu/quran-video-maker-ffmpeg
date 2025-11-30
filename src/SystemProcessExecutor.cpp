#include "SystemProcessExecutor.h"
#include <iostream>
#include <cstdio>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#define QVM_POPEN _popen
#define QVM_PCLOSE _pclose
#else
#define QVM_POPEN popen
#define QVM_PCLOSE pclose
#endif

namespace {

std::string trim(const std::string& input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }
    if (start == input.size()) return "";
    size_t end = input.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(input[end]))) {
        --end;
    }
    return input.substr(start, end - start + 1);
}

void emitProgressEvent(const std::string& stage,
                       const std::string& status,
                       double percent = -1.0,
                       double elapsedSeconds = -1.0,
                       double etaSeconds = -1.0,
                       const std::string& message = "") {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2);
    oss << "PROGRESS {\"stage\":\"" << stage << "\",\"status\":\"" << status << "\"";
    if (percent >= 0.0) oss << ",\"percent\":" << percent;
    if (elapsedSeconds >= 0.0) oss << ",\"elapsedSeconds\":" << elapsedSeconds;
    if (etaSeconds >= 0.0) oss << ",\"etaSeconds\":" << etaSeconds;
    if (!message.empty()) oss << ",\"message\":\"" << message << "\"";
    oss << "}";
    std::cout << oss.str() << std::endl;
}

double parseOutTimeValue(const std::string& value) {
    try {
        return std::stod(value) / 1000000.0;
    } catch (...) {
        return 0.0;
    }
}

} // namespace

int SystemProcessExecutor::execute(const std::string& command) {
    return system(command.c_str());
}

void SystemProcessExecutor::executeWithProgress(const std::string& command, double totalDurationSeconds) {
    auto startTime = std::chrono::steady_clock::now();
    emitProgressEvent("encoding", "running", 0.0, 0.0, -1.0, "FFmpeg started");

    FILE* pipe = QVM_POPEN(command.c_str(), "r");
    if (!pipe) {
        emitProgressEvent("encoding", "failed", 0.0, 0.0, -1.0, "Failed to start FFmpeg");
        throw std::runtime_error("Failed to start FFmpeg process");
    }

    char buffer[512];
    double lastOutSeconds = 0.0;
    double lastPercent = 0.0;
    bool sawProgress = false;

    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line = trim(buffer);
        if (line.empty()) continue;
        auto delimiter = line.find('=');
        if (delimiter == std::string::npos) continue;
        std::string key = trim(line.substr(0, delimiter));
        std::string value = trim(line.substr(delimiter + 1));

        if (key == "out_time_ms") {
            lastOutSeconds = parseOutTimeValue(value);
        } else if (key == "speed") {
            // Parse speed for informational purposes if we want to expose it later.
            continue;
        } else if (key == "progress") {
            sawProgress = true;
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            double percent = (totalDurationSeconds > 0.0)
                ? std::clamp((lastOutSeconds / totalDurationSeconds) * 100.0, 0.0, 100.0)
                : -1.0;
            lastPercent = percent >= 0.0 ? percent : lastPercent;
            double eta = -1.0;
            if (percent > 0.0 && percent < 100.0) {
                double ratio = percent / 100.0;
                eta = elapsed * ((1.0 - ratio) / ratio);
            } else if (percent >= 100.0) {
                eta = 0.0;
            }

            const bool finished = (value == "end");
            emitProgressEvent("encoding",
                              finished ? "completed" : "running",
                              percent,
                              elapsed,
                              eta,
                              finished ? "Encoding complete" : "Encoding in progress");
            if (finished) break;
        }
    }

    int exitCode = QVM_PCLOSE(pipe);
    if (exitCode != 0) {
        emitProgressEvent("encoding", "failed", lastPercent, -1.0, -1.0, "FFmpeg exited with error");
        throw std::runtime_error("FFmpeg execution failed");
    }

    if (!sawProgress) {
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
        emitProgressEvent("encoding", "completed", 100.0, elapsed, 0.0, "Encoding complete");
    }
}
