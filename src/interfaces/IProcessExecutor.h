#pragma once
#include <string>

namespace Interfaces {
    class IProcessExecutor {
    public:
        virtual ~IProcessExecutor() = default;
        virtual int execute(const std::string& command) = 0;
        virtual void executeWithProgress(const std::string& command, double totalDurationSeconds) = 0;
    };
}
