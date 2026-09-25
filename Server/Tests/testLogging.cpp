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

bool process_and_append(
    CommandProcessor& processor,
    AOFLogger& logger,
    const std::string& command
) {
    const CommandProcessResult result = processor.process(command);
    if (!result.is_success()) {
        ADD_FAILURE() << "Command was not parsed: " << command;
        return false;
    }
    if (!result.processed_command().execution_result.success) {
        ADD_FAILURE() << "Command failed: " << command << ": "
                      << result.processed_command().execution_result.message;
        return false;
    }
    if (result.processed_command().should_log) {
        logger.append_record(result.processed_command().aof_record);
    }
    return true;
}

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
        logger.append_record(RespCommandCodec::encode({"PEXPIREAT", "user", "4102444800000"}));
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

TEST(LoggingTest, LogRunnerDoesNotRenewAnExpirationThatPassedWhileStopped) {
    TempLogFile log_file("expired-during-downtime");

    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(RespCommandCodec::encode({"SET", "session", "token"}));
        logger.append_record(RespCommandCodec::encode({"PEXPIREAT", "session", "0"}));
    }

    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor command_processor(executor);

    LogRunner(log_file.path_string()).run_log(command_processor);

    EXPECT_FALSE(storage.exists("session"));
}

TEST(LoggingTest, LogRunnerReplaysPersistAfterExpiration) {
    TempLogFile log_file("persist-replay");
    {
        AOFLogger logger(log_file.path_string());
        logger.append_record(RespCommandCodec::encode({"SET", "session", "token"}));
        logger.append_record(RespCommandCodec::encode({"PEXPIREAT", "session", "4102444800000"}));
        logger.append_record(RespCommandCodec::encode({"PERSIST", "session"}));
    }

    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);
    LogRunner(log_file.path_string()).run_log(processor);

    EXPECT_TRUE(storage.exists("session"));
    EXPECT_EQ(storage.ttl_milliseconds("session"), -1);
}

TEST(LoggingTest, ExpireIsSerializedAsAnAbsoluteUnixMillisecondDeadline) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    const auto before = std::chrono::system_clock::now();

    const auto result = command_processor.process("EXPIRE \"session\" 30");
    const auto after = std::chrono::system_clock::now();

    ASSERT_TRUE(result.is_success());
    const auto decoded = RespCommandCodec::decode(result.processed_command().aof_record);
    ASSERT_EQ(decoded.status, RespDecodeStatus::COMPLETE);
    ASSERT_EQ(decoded.arguments.size(), 3U);
    EXPECT_EQ(decoded.arguments[0], "PEXPIREAT");
    EXPECT_EQ(decoded.arguments[1], "session");
    const auto deadline = std::stoll(decoded.arguments[2]);
    const auto earliest = std::chrono::duration_cast<std::chrono::milliseconds>(
        (before + std::chrono::seconds(30)).time_since_epoch()).count();
    const auto latest = std::chrono::duration_cast<std::chrono::milliseconds>(
        (after + std::chrono::seconds(30)).time_since_epoch()).count();
    EXPECT_GE(deadline, earliest);
    EXPECT_LE(deadline, latest);
}

TEST(LoggingTest, NoOpPersistDoesNotProduceAnAofRecord) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const auto missing = processor.process("PERSIST \"missing\"");
    storage.set("permanent", Value("value"));
    const auto permanent = processor.process("PERSIST \"permanent\"");

    ASSERT_TRUE(missing.is_success());
    ASSERT_TRUE(permanent.is_success());
    EXPECT_FALSE(missing.processed_command().should_log);
    EXPECT_FALSE(permanent.processed_command().should_log);
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

TEST(LoggingTest, GeneratedNumericAofRecordsRestoreTheResult) {
    TempLogFile log_file("numeric-replay");
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AOFLogger logger(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, logger, "SET \"counter\" 10"));
        ASSERT_TRUE(process_and_append(processor, logger, "INCR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, logger, "DECR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, logger, "INCRBY \"counter\" 5"));
        ASSERT_TRUE(process_and_append(processor, logger, "DECRBY \"counter\" 3"));
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "12");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}

TEST(LoggingTest, NumericReplayPreservesAFutureExpiration) {
    TempLogFile log_file("numeric-future-expiration");
    const auto deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::minutes(1)).time_since_epoch()).count();
    const StorageEngine::TimePoint deadline{
        std::chrono::duration_cast<StorageEngine::Duration>(
            std::chrono::milliseconds(deadline_milliseconds))};
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AOFLogger logger(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, logger, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline));
        logger.append_record(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, logger, "INCR \"counter\""));
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_GT(replayed_storage.ttl_milliseconds("counter"), 0);
}

TEST(LoggingTest, NumericReplayDoesNotRecreateAKeyThatExpiredDuringDowntime) {
    TempLogFile log_file("numeric-expired-during-downtime");
    const auto deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::milliseconds(250)).time_since_epoch()).count();
    const StorageEngine::TimePoint deadline{
        std::chrono::duration_cast<StorageEngine::Duration>(
            std::chrono::milliseconds(deadline_milliseconds))};
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AOFLogger logger(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, logger, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline));
        logger.append_record(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, logger, "INCR \"counter\""));
    }
    std::this_thread::sleep_until(deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);

    EXPECT_FALSE(replayed_storage.exists("counter"));
}

TEST(LoggingTest, NumericReplayPreservesAKeyRecreatedAfterExpiration) {
    TempLogFile log_file("numeric-recreated-after-expiration");
    const auto deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::milliseconds(25)).time_since_epoch()).count();
    const StorageEngine::TimePoint deadline{
        std::chrono::duration_cast<StorageEngine::Duration>(
            std::chrono::milliseconds(deadline_milliseconds))};
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AOFLogger logger(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, logger, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline));
        logger.append_record(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        std::this_thread::sleep_until(deadline + std::chrono::milliseconds(25));
        ASSERT_TRUE(process_and_append(processor, logger, "INCR \"counter\""));
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "1");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}

TEST(LoggingTest, PersistAfterNumericMutationSurvivesTheOldDeadline) {
    TempLogFile log_file("numeric-then-persist");
    const auto deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::milliseconds(250)).time_since_epoch()).count();
    const StorageEngine::TimePoint deadline{
        std::chrono::duration_cast<StorageEngine::Duration>(
            std::chrono::milliseconds(deadline_milliseconds))};
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AOFLogger logger(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, logger, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline));
        logger.append_record(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, logger, "INCR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, logger, "PERSIST \"counter\""));
    }
    std::this_thread::sleep_until(deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}

TEST(LoggingTest, ExtendingExpirationAfterNumericMutationSurvivesTheOldDeadline) {
    TempLogFile log_file("numeric-then-extend-expiration");
    const auto old_deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::milliseconds(250)).time_since_epoch()).count();
    const StorageEngine::TimePoint old_deadline{
        std::chrono::duration_cast<StorageEngine::Duration>(
            std::chrono::milliseconds(old_deadline_milliseconds))};
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AOFLogger logger(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, logger, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", old_deadline));
        logger.append_record(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(old_deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, logger, "INCR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, logger, "EXPIRE \"counter\" 60"));
    }
    std::this_thread::sleep_until(old_deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_GT(replayed_storage.ttl_milliseconds("counter"), 0);
}
