#pragma once
#include "interfaces/IProcessExecutor.h"

class SystemProcessExecutor : public Interfaces::IProcessExecutor {
public:
    int execute(const std::string& command) override;
    void executeWithProgress(const std::string& command, double totalDurationSeconds) override;
};
