#include "App/CommandProcessor.h"
#include "Commands/Executor.h"
#include "Persistence/LogCompactor.h"
#include "Persistence/LogRunner.h"
#include "Protocol/RespCommandCodec.h"
#include "Storage/StorageEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace {

std::filesystem::path make_temp_log_path(const std::string& test_name) {
    const auto temp_dir = std::filesystem::temp_directory_path();
    return temp_dir / ("redisimpl-logcompactor-" + test_name + ".txt");
}

class TempLogFile {
public:
    TempLogFile(const std::string& test_name, const std::string& contents)
        : path_(make_temp_log_path(test_name)) {
        std::ofstream stream(path_);
        stream << contents;
    }

    ~TempLogFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    std::string path_string() const {
        return path_.string();
    }

    std::string read_all() const {
        std::ifstream stream(path_);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

private:
    std::filesystem::path path_;
};

void compact_log(const std::string& path) {
    LogCompactor compactor(path);
    compactor.compact();
}

} // namespace

TEST(LogCompactorTest, RemovesOlderSetForSameKey) {
    TempLogFile log_file(
        "older-set",
        RespCommandCodec::encode({"SET", "user", "alice"}) +
        RespCommandCodec::encode({"SET", "counter", "1"}) +
        RespCommandCodec::encode({"SET", "user", "bob"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"SET", "counter", "1"}) +
        RespCommandCodec::encode({"SET", "user", "bob"}));
}

TEST(LogCompactorTest, KeepsLatestExpireForSameKey) {
    TempLogFile log_file(
        "latest-expire",
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444800000"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444860000"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444860000"}));
}

TEST(LogCompactorTest, PersistReplacesPriorExpiration) {
    TempLogFile log_file(
        "persist-replaces-expire",
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "0"}) +
        RespCommandCodec::encode({"PERSIST", "session"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(log_file.read_all(),
              RespCommandCodec::encode({"SET", "session", "token"}) +
              RespCommandCodec::encode({"PERSIST", "session"}));
}

TEST(LogCompactorTest, DeleteRemovesPriorMutationsForKey) {
    TempLogFile log_file(
        "delete-key",
        RespCommandCodec::encode({"SET", "session", "token"}) +
        RespCommandCodec::encode({"PEXPIREAT", "session", "4102444800000"}) +
        RespCommandCodec::encode({"DEL", "session"}) +
        RespCommandCodec::encode({"SET", "other", "value"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"DEL", "session"}) +
        RespCommandCodec::encode({"SET", "other", "value"}));
}

TEST(LogCompactorTest, LeavesIndependentKeysInOriginalOrder) {
    TempLogFile log_file(
        "independent-keys",
        RespCommandCodec::encode({"SET", "a", "1"}) +
        RespCommandCodec::encode({"SET", "b", "2"}) +
        RespCommandCodec::encode({"PEXPIREAT", "a", "4102444800000"}) +
        RespCommandCodec::encode({"SET", "c", "3"}));

    compact_log(log_file.path_string());

    EXPECT_EQ(
        log_file.read_all(),
        RespCommandCodec::encode({"SET", "a", "1"}) +
        RespCommandCodec::encode({"SET", "b", "2"}) +
        RespCommandCodec::encode({"PEXPIREAT", "a", "4102444800000"}) +
        RespCommandCodec::encode({"SET", "c", "3"}));
}

TEST(LogCompactorTest, PreservesBinaryKeyAndValue) {
    const Bytes key{"k\0\n", 3};
    const Bytes old_value{"old\0", 4};
    const Bytes new_value{"new\xff", 4};
    TempLogFile log_file("binary", RespCommandCodec::encode({"SET", key, old_value}) +
                                   RespCommandCodec::encode({"SET", key, new_value}));
    LogCompactor(log_file.path_string()).compact();
    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", key, new_value}));
}

TEST(LogCompactorTest, LaterSetRemovesPriorExpirationAndDelete) {
    TempLogFile log_file("set-resets-state",
        RespCommandCodec::encode({"SET", "key", "old"}) +
        RespCommandCodec::encode({"PEXPIREAT", "key", "4102444800000"}) +
        RespCommandCodec::encode({"DEL", "key"}) +
        RespCommandCodec::encode({"SET", "key", "new"}));
    LogCompactor(log_file.path_string()).compact();
    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "key", "new"}));
}

TEST(LogCompactorTest, CompactingNumericMutationsPreservesFinalState) {
    Bytes contents;
    StorageEngine source_storage;
    Executor source_executor(source_storage);
    CommandProcessor source_processor(source_executor);
    for (const std::string command : {
             "SET \"counter\" 10", "INCR \"counter\"", "INCRBY \"counter\" 5",
             "DECR \"counter\"", "SET \"removed\" 1", "DECRBY \"removed\" 2",
             "DEL \"removed\""}) {
        const CommandProcessResult result = source_processor.process(command);
        ASSERT_TRUE(result.is_success()) << command;
        ASSERT_TRUE(result.processed_command().execution_result.success) << command;
        if (result.processed_command().should_log) {
            contents += result.processed_command().aof_record;
        }
    }
    TempLogFile log_file("numeric-final-state", contents);

    LogCompactor(log_file.path_string()).compact();

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);
    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "15");
    EXPECT_FALSE(replayed_storage.exists("removed"));
}

TEST(LogCompactorTest, CompactingNumericMutationsPreservesExpirationState) {
    Bytes contents;
    StorageEngine source_storage;
    Executor source_executor(source_storage);
    CommandProcessor source_processor(source_executor);

    const CommandProcessResult set_result = source_processor.process("SET \"counter\" 10");
    ASSERT_TRUE(set_result.is_success());
    ASSERT_TRUE(set_result.processed_command().execution_result.success);
    contents += set_result.processed_command().aof_record;

    const auto deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::minutes(1)).time_since_epoch()).count();
    const CommandProcessResult expire_result = source_processor.process_arguments(
        {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)});
    ASSERT_TRUE(expire_result.is_success());
    ASSERT_TRUE(expire_result.processed_command().execution_result.success);
    contents += expire_result.processed_command().aof_record;

    const CommandProcessResult increment_result = source_processor.process("INCR \"counter\"");
    ASSERT_TRUE(increment_result.is_success());
    ASSERT_TRUE(increment_result.processed_command().execution_result.success);
    contents += increment_result.processed_command().aof_record;

    TempLogFile log_file("numeric-expiration-state", contents);
    LogCompactor(log_file.path_string()).compact();

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);
    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_GT(replayed_storage.ttl_milliseconds("counter"), 0);
}

TEST(LogCompactorTest, NumericStateFollowedByPersistSurvivesCompactionAndOldExpiration) {
    Bytes contents;
    StorageEngine source_storage;
    Executor source_executor(source_storage);
    CommandProcessor source_processor(source_executor);
    const auto old_deadline_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            (StorageEngine::Clock::now() + std::chrono::milliseconds(250)).time_since_epoch()).count();
    const StorageEngine::TimePoint old_deadline{
        std::chrono::duration_cast<StorageEngine::Duration>(
            std::chrono::milliseconds(old_deadline_milliseconds))};

    const auto set = source_processor.process("SET \"counter\" 10");
    ASSERT_TRUE(set.is_success());
    contents += set.processed_command().aof_record;
    ASSERT_TRUE(source_storage.expire_at("counter", old_deadline).applied);
    contents += RespCommandCodec::encode(
        {"PEXPIREAT", "counter", std::to_string(old_deadline_milliseconds)});
    const auto increment = source_processor.process("INCR \"counter\"");
    ASSERT_TRUE(increment.is_success());
    ASSERT_TRUE(increment.processed_command().execution_result.success);
    contents += increment.processed_command().aof_record;
    const auto persist = source_processor.process("PERSIST \"counter\"");
    ASSERT_TRUE(persist.is_success());
    ASSERT_TRUE(persist.processed_command().execution_result.success);
    contents += persist.processed_command().aof_record;
    TempLogFile log_file("numeric-persist-after-compaction", contents);

    LogCompactor(log_file.path_string()).compact();
    std::this_thread::sleep_until(old_deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    CommandProcessor replayed_processor(replayed_executor);
    LogRunner(log_file.path_string()).run_log(replayed_processor);
    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}
