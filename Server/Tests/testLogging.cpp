#include "CommandProcessor.h"
#include "AOFLogger.h"
#include "LogRunner.h"
#include "Executor.h"
#include "StorageEngine.h"
#include "RespCommandCodec.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

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

TEST(LoggingTest, AofLoggerWritesRecordExactly) {
    TempLogFile log_file("exact-record");
    const Bytes record = RespCommandCodec::encode({"SET", "name", "mark"});

    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(record);
    }

    EXPECT_EQ(log_file.read_all(), record);
}

TEST(LoggingTest, AofLoggerAppendsAfterReopening) {
    TempLogFile log_file("reopen-append");

    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(RespCommandCodec::encode({"SET", "first", "1"}));
    }
    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(RespCommandCodec::encode({"SET", "second", "2"}));
    }

    EXPECT_EQ(log_file.read_all(),
              RespCommandCodec::encode({"SET", "first", "1"}) +
              RespCommandCodec::encode({"SET", "second", "2"}));
}

TEST(LoggingTest, AofLoggerSupportsEverySecondPolicy) {
    TempLogFile log_file("every-second");

    {
        AOFLogger logger(log_file.path_string(), AOFFsyncPolicy::EVERY_SECOND);
        logger.append_record(RespCommandCodec::encode({"SET", "key", "value"}));
    }

    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "key", "value"}));
}

TEST(LoggingTest, AofLoggerSupportsNeverPolicy) {
    TempLogFile log_file("never-sync");

    {
        AOFLogger logger(log_file.path_string(), AOFFsyncPolicy::NEVER);
        logger.append_record(RespCommandCodec::encode({"SET", "key", "value"}));
    }

    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "key", "value"}));
}

TEST(LoggingTest, AofLoggerWritesLargeEntriesCompletely) {
    TempLogFile log_file("large-entry");
    const Bytes entry = RespCommandCodec::encode({"SET", "large", Bytes(1024 * 1024, 'x')});

    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(entry);
    }

    EXPECT_EQ(log_file.read_all(), entry);
}

TEST(LoggingTest, AofLoggerThrowsWhenParentDirectoryDoesNotExist) {
    const std::filesystem::path missing_path =
        std::filesystem::temp_directory_path() / "redisimpl-missing-directory" / "aof.log";
    std::filesystem::remove_all(missing_path.parent_path());

    EXPECT_THROW(AOFLogger logger(missing_path.string()), std::runtime_error);
}

TEST(LoggingTest, AofLoggerSerializesConcurrentWrites) {
    TempLogFile log_file("concurrent-writes");
    constexpr int kThreadCount = 4;
    constexpr int kEntriesPerThread = 50;

    {
        AOFLogger logger(log_file.path_string());
        std::vector<std::thread> threads;
        for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
            threads.emplace_back([&logger, thread_id] {
                for (int entry = 0; entry < kEntriesPerThread; ++entry) {
                    logger.append_record(RespCommandCodec::encode(
                        {"SET", std::to_string(thread_id) + ":" + std::to_string(entry), "value"}));
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    Bytes contents = log_file.read_all();
    std::unordered_set<std::string> entries;
    while (!contents.empty()) {
        const auto decoded = RespCommandCodec::decode(contents);
        ASSERT_EQ(decoded.status, RespDecodeStatus::COMPLETE);
        entries.insert(decoded.arguments[1]);
        contents.erase(0, decoded.bytes_consumed);
    }

    EXPECT_EQ(entries.size(), static_cast<std::size_t>(kThreadCount * kEntriesPerThread));
}

TEST(LoggingTest, LogRunnerReplaysMutatingCommandsIntoStorage) {
    TempLogFile log_file("replay");

    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(RespCommandCodec::encode({"SET", "user", "alice"}));
        logger.append_record(RespCommandCodec::encode({"EXPIRE", "user", "30"}));
    }

    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    LogRunner runner(log_file.path_string());

    runner.run_log(command_processor);

    ASSERT_TRUE(storage.get("user").has_value());
    EXPECT_EQ(storage.get("user")->bytes(), "alice");
    EXPECT_TRUE(storage.exists("user"));
}

TEST(LoggingTest, LogRunnerThrowsForMalformedLogEntry) {
    TempLogFile log_file("invalid-entry");

    {
        std::ofstream stream(log_file.path_string(), std::ios::binary);
        stream << "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$5\r\nabc";
    }

    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    LogRunner runner(log_file.path_string());

    EXPECT_THROW(runner.run_log(command_processor), std::runtime_error);
}

TEST(LoggingTest, BinaryKeyAndValueSurviveReplay) {
    TempLogFile log_file("binary-replay");
    const Bytes key{"\0k\n\xff", 4};
    const Bytes value{"v\"\r\n\0\xff", 6};
    { AOFLogger logger(log_file.path_string()); logger.append_record(RespCommandCodec::encode({"SET", key, value})); }
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);
    LogRunner(log_file.path_string()).run_log(processor);
    ASSERT_TRUE(storage.get(key).has_value());
    EXPECT_EQ(storage.get(key)->bytes(), value);
}

TEST(LoggingTest, LogRunnerRejectsNonMutatingCommands) {
    TempLogFile log_file("read-command");
    { AOFLogger logger(log_file.path_string()); logger.append_record(RespCommandCodec::encode({"GET", "key"})); }
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);
    EXPECT_THROW(LogRunner(log_file.path_string()).run_log(processor), std::runtime_error);
}
