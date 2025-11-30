#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include "SystemProcessExecutor.h"
#include "interfaces/IProcessExecutor.h"

class ReciterListingTest : public ::testing::Test {
protected:
    std::unique_ptr<Interfaces::IProcessExecutor> processExecutor;

    void SetUp() override {
        processExecutor = std::make_unique<SystemProcessExecutor>();
    }

    std::string runCommand(const std::string& command) {
        const std::string tempFile = "reciter_list.txt";
        std::string redirectedCommand = command + " > " + tempFile;
        int exitCode = processExecutor->execute(redirectedCommand);
        if (exitCode != 0) {
            throw std::runtime_error("Command failed with exit code " + std::to_string(exitCode));
        }
        std::ifstream file(tempFile);
        std::string output((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::remove(tempFile.c_str());
        return output;
    }
};

TEST_F(ReciterListingTest, ListRecitersCommand) {
    std::string command = "./qvm --list-reciters";
    std::string output = runCommand(command);

    ASSERT_THAT(output, testing::HasSubstr("Abdur Rahman as-Sudais"));
    ASSERT_THAT(output, testing::HasSubstr("Mishari Rashid al-Afasy"));
    ASSERT_THAT(output, testing::HasSubstr("Saad al-Ghamdi"));
}
