#pragma once
#include "interfaces/IProcessExecutor.h"
#include <vector>
#include <string>

class MockProcessExecutor : public Interfaces::IProcessExecutor {
public:
    int execute(const std::string& command) override {
        commands.push_back(command);
        return 0;
    }

    void executeWithProgress(const std::string& command, double totalDurationSeconds) override {
        commands.push_back(command);
    }

    const std::vector<std::string>& getCommands() const {
        return commands;
    }

private:
    std::vector<std::string> commands;
};
