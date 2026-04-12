#include "Log/AOFLogger.h"
#include "Log/LogRunner.h"
#include "Interpreter/Runtime/CommandProcessor.h"
#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Runtime/StorageEngine.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path make_temp_log_path(const std::string& test_name) {
    const auto temp_dir = std::filesystem::temp_directory_path();
    return temp_dir / ("redisimpl-" + test_name + "-log.txt");
}

class TempLogFile {
public:
    explicit TempLogFile(const std::string& test_name) : path_(make_temp_log_path(test_name)) {
        std::filesystem::remove(path_);
    }

    ~TempLogFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::string path_string() const {
        return path_.string();
    }

    std::string read_all() const {
        std::ifstream stream(path_);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

private:
    std::filesystem::path path_;
};

} // namespace

TEST(LoggingTest, AofLoggerAppendsTrailingNewlineWhenNeeded) {
    TempLogFile log_file("append-newline");

    {
        AOFLogger logger(log_file.path_string());
        logger.enqueue("SET \"name\" \"mark\"");
    }

    EXPECT_EQ(log_file.read_all(), "SET \"name\" \"mark\"\n");
}

TEST(LoggingTest, AofLoggerLeavesExistingNewlineUntouched) {
    TempLogFile log_file("preserve-newline");

    {
        AOFLogger logger(log_file.path_string());
        logger.enqueue("DEL \"name\"\n");
    }

    EXPECT_EQ(log_file.read_all(), "DEL \"name\"\n");
}

TEST(LoggingTest, LogRunnerReplaysMutatingCommandsIntoStorage) {
    TempLogFile log_file("replay");

    {
        AOFLogger logger(log_file.path_string());
        logger.enqueue("SET \"user\" \"alice\"");
        logger.enqueue("EXPIRE \"user\" 30");
    }

    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    LogRunner runner(log_file.path_string());

    runner.run_log(command_processor);

    ASSERT_TRUE(storage.get("user").has_value());
    EXPECT_EQ(storage.get("user")->get<std::string>(), "alice");
    EXPECT_TRUE(storage.exists("user"));
}

TEST(LoggingTest, LogRunnerThrowsForMalformedLogEntry) {
    TempLogFile log_file("invalid-entry");

    {
        std::ofstream stream(log_file.path_string());
        stream << "SET \"user\"\n";
    }

    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    LogRunner runner(log_file.path_string());

    EXPECT_THROW(runner.run_log(command_processor), std::runtime_error);
}
