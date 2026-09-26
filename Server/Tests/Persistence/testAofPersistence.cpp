#include "App/CommandProcessor.h"
#include "Persistence/AofWriter.h"
#include "Persistence/AofReplayer.h"
#include "Commands/Executor.h"
#include "Storage/StorageEngine.h"
#include "Protocol/RespCommandCodec.h"

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
    AofWriter& aof_writer,
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
        aof_writer.append(result.processed_command().aof_record);
    }
    return true;
}

} // namespace

TEST(AofPersistenceTest, AofWriterWritesRecordExactly) {
    TempLogFile log_file("exact-record");
    const Bytes record = RespCommandCodec::encode({"SET", "name", "mark"});

    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(record);
    }

    EXPECT_EQ(log_file.read_all(), record);
}

TEST(AofPersistenceTest, AofWriterAppendsAfterReopening) {
    TempLogFile log_file("reopen-append");

    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(RespCommandCodec::encode({"SET", "first", "1"}));
    }
    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(RespCommandCodec::encode({"SET", "second", "2"}));
    }

    EXPECT_EQ(log_file.read_all(),
              RespCommandCodec::encode({"SET", "first", "1"}) +
              RespCommandCodec::encode({"SET", "second", "2"}));
}

TEST(AofPersistenceTest, AofWriterSupportsEverySecondPolicy) {
    TempLogFile log_file("every-second");

    {
        AofWriter aof_writer(log_file.path_string(), AofFsyncPolicy::EVERY_SECOND);
        aof_writer.append(RespCommandCodec::encode({"SET", "key", "value"}));
    }

    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "key", "value"}));
}

TEST(AofPersistenceTest, AofWriterSupportsNeverPolicy) {
    TempLogFile log_file("never-sync");

    {
        AofWriter aof_writer(log_file.path_string(), AofFsyncPolicy::NEVER);
        aof_writer.append(RespCommandCodec::encode({"SET", "key", "value"}));
    }

    EXPECT_EQ(log_file.read_all(), RespCommandCodec::encode({"SET", "key", "value"}));
}

TEST(AofPersistenceTest, AofWriterWritesLargeEntriesCompletely) {
    TempLogFile log_file("large-entry");
    const Bytes entry = RespCommandCodec::encode({"SET", "large", Bytes(1024 * 1024, 'x')});

    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(entry);
    }

    EXPECT_EQ(log_file.read_all(), entry);
}

TEST(AofPersistenceTest, AofWriterThrowsWhenParentDirectoryDoesNotExist) {
    const std::filesystem::path missing_path =
        std::filesystem::temp_directory_path() / "redisimpl-missing-directory" / "aof.log";
    std::filesystem::remove_all(missing_path.parent_path());

    EXPECT_THROW(AofWriter aof_writer(missing_path.string()), std::runtime_error);
}

TEST(AofPersistenceTest, AofWriterSerializesConcurrentWrites) {
    TempLogFile log_file("concurrent-writes");
    constexpr int kThreadCount = 4;
    constexpr int kEntriesPerThread = 50;

    {
        AofWriter aof_writer(log_file.path_string());
        std::vector<std::thread> threads;
        for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
            threads.emplace_back([&aof_writer, thread_id] {
                for (int entry = 0; entry < kEntriesPerThread; ++entry) {
                    aof_writer.append(RespCommandCodec::encode(
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

TEST(AofPersistenceTest, AofReplayerReplaysMutatingCommandsIntoStorage) {
    TempLogFile log_file("replay");

    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(RespCommandCodec::encode({"SET", "user", "alice"}));
        aof_writer.append(RespCommandCodec::encode({"PEXPIREAT", "user", "4102444800000"}));
    }

    StorageEngine storage;
    Executor executor(storage);
    AofReplayer runner(log_file.path_string());

    runner.replay(executor);

    ASSERT_TRUE(storage.get("user").has_value());
    EXPECT_EQ(storage.get("user")->bytes(), "alice");
    EXPECT_TRUE(storage.exists("user"));
}

TEST(AofPersistenceTest, AofReplayerDoesNotRenewAnExpirationThatPassedWhileStopped) {
    TempLogFile log_file("expired-during-downtime");

    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(RespCommandCodec::encode({"SET", "session", "token"}));
        aof_writer.append(RespCommandCodec::encode({"PEXPIREAT", "session", "0"}));
    }

    StorageEngine storage;
    Executor executor(storage);

    AofReplayer(log_file.path_string()).replay(executor);

    EXPECT_FALSE(storage.exists("session"));
}

TEST(AofPersistenceTest, AofReplayerReplaysPersistAfterExpiration) {
    TempLogFile log_file("persist-replay");
    {
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(RespCommandCodec::encode({"SET", "session", "token"}));
        aof_writer.append(RespCommandCodec::encode({"PEXPIREAT", "session", "4102444800000"}));
        aof_writer.append(RespCommandCodec::encode({"PERSIST", "session"}));
    }

    StorageEngine storage;
    Executor executor(storage);
    AofReplayer(log_file.path_string()).replay(executor);

    EXPECT_TRUE(storage.exists("session"));
    EXPECT_EQ(storage.ttl_milliseconds("session"), -1);
}

TEST(AofPersistenceTest, ExpireIsLoggedAsOneStateRecordWithAnAbsoluteDeadline) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    Executor executor(storage);
    CommandProcessor command_processor(executor);
    const auto before = std::chrono::system_clock::now();

    const auto result = command_processor.process("EXPIRE \"session\" 30");
    const auto after = std::chrono::system_clock::now();

    ASSERT_TRUE(result.is_success());
    const Bytes& aof_record = result.processed_command().aof_record;
    const auto decoded = RespCommandCodec::decode(aof_record);
    ASSERT_EQ(decoded.status, RespDecodeStatus::COMPLETE);
    EXPECT_EQ(decoded.bytes_consumed, aof_record.size()) << "EXPIRE should log exactly one record";
    ASSERT_EQ(decoded.arguments.size(), 4U);
    EXPECT_EQ(decoded.arguments[0], "SETSTATE");
    EXPECT_EQ(decoded.arguments[1], "session");
    EXPECT_EQ(decoded.arguments[2], "token");
    const auto deadline = std::stoll(decoded.arguments[3]);
    const auto earliest = std::chrono::duration_cast<std::chrono::milliseconds>(
        (before + std::chrono::seconds(30)).time_since_epoch()).count();
    const auto latest = std::chrono::duration_cast<std::chrono::milliseconds>(
        (after + std::chrono::seconds(30)).time_since_epoch()).count();
    EXPECT_GE(deadline, earliest);
    EXPECT_LE(deadline, latest);
}

TEST(AofPersistenceTest, ExpireInThePastIsLoggedAsPexpireatAndDeletesOnReplay) {
    TempLogFile log_file("expire-in-past");
    {
        StorageEngine storage;
        storage.set("session", Value("token"));
        Executor executor(storage);
        CommandProcessor command_processor(executor);
        AofWriter aof_writer(log_file.path_string());
        aof_writer.append(RespCommandCodec::encode({"SET", "session", "token"}));

        const auto result = command_processor.process("EXPIRE session -1");
        ASSERT_TRUE(result.is_success());
        const auto decoded = RespCommandCodec::decode(result.processed_command().aof_record);
        ASSERT_EQ(decoded.status, RespDecodeStatus::COMPLETE);
        EXPECT_EQ(decoded.arguments[0], "PEXPIREAT");
        aof_writer.append(result.processed_command().aof_record);
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);
    EXPECT_FALSE(replayed_storage.exists("session"));
}

TEST(AofPersistenceTest, NoOpPersistDoesNotProduceAnAofRecord) {
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

TEST(AofPersistenceTest, AofReplayerDropsIncompleteFinalRecord) {
    TempLogFile log_file("truncated-tail");
    const Bytes complete = RespCommandCodec::encode({"SET", "a", "1"});
    const Bytes interrupted = RespCommandCodec::encode({"SET", "b", "12345"});
    {
        std::ofstream stream(log_file.path_string(), std::ios::binary);
        stream << complete << interrupted.substr(0, interrupted.size() - 4);
    }

    StorageEngine storage;
    Executor executor(storage);
    AofReplayer(log_file.path_string()).replay(executor);

    ASSERT_TRUE(storage.get("a").has_value());
    EXPECT_EQ(storage.get("a")->bytes(), "1");
    EXPECT_FALSE(storage.exists("b"));
    // The partial record is cut off the file, so later appends follow a complete record.
    EXPECT_EQ(log_file.read_all(), complete);

    { AofWriter aof_writer(log_file.path_string()); aof_writer.append(RespCommandCodec::encode({"SET", "c", "3"})); }
    StorageEngine restarted_storage;
    Executor restarted_executor(restarted_storage);
    AofReplayer(log_file.path_string()).replay(restarted_executor);
    EXPECT_TRUE(restarted_storage.exists("a"));
    EXPECT_TRUE(restarted_storage.exists("c"));
}

TEST(AofPersistenceTest, AofReplayerThrowsForMalformedRecord) {
    TempLogFile log_file("invalid-entry");
    {
        std::ofstream stream(log_file.path_string(), std::ios::binary);
        stream << "!not resp\r\n" << RespCommandCodec::encode({"SET", "a", "1"});
    }

    StorageEngine storage;
    Executor executor(storage);
    EXPECT_THROW(AofReplayer(log_file.path_string()).replay(executor), std::runtime_error);
}

TEST(AofPersistenceTest, BinaryKeyAndValueSurviveReplay) {
    TempLogFile log_file("binary-replay");
    const Bytes key{"\0k\n\xff", 4};
    const Bytes value{"v\"\r\n\0\xff", 6};
    { AofWriter aof_writer(log_file.path_string()); aof_writer.append(RespCommandCodec::encode({"SET", key, value})); }
    StorageEngine storage;
    Executor executor(storage);
    AofReplayer(log_file.path_string()).replay(executor);
    ASSERT_TRUE(storage.get(key).has_value());
    EXPECT_EQ(storage.get(key)->bytes(), value);
}

TEST(AofPersistenceTest, AofReplayerRejectsNonMutatingCommands) {
    TempLogFile log_file("read-command");
    { AofWriter aof_writer(log_file.path_string()); aof_writer.append(RespCommandCodec::encode({"GET", "key"})); }
    StorageEngine storage;
    Executor executor(storage);
    EXPECT_THROW(AofReplayer(log_file.path_string()).replay(executor), std::runtime_error);
}

TEST(AofPersistenceTest, GeneratedNumericAofRecordsRestoreTheResult) {
    TempLogFile log_file("numeric-replay");
    {
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);
        AofWriter aof_writer(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, aof_writer, "SET \"counter\" 10"));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "DECR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCRBY \"counter\" 5"));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "DECRBY \"counter\" 3"));
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "12");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}

TEST(AofPersistenceTest, NumericReplayPreservesAFutureExpiration) {
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
        AofWriter aof_writer(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, aof_writer, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline).applied);
        aof_writer.append(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCR \"counter\""));
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_GT(replayed_storage.ttl_milliseconds("counter"), 0);
}

TEST(AofPersistenceTest, NumericReplayDoesNotRecreateAKeyThatExpiredDuringDowntime) {
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
        AofWriter aof_writer(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, aof_writer, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline).applied);
        aof_writer.append(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCR \"counter\""));
    }
    std::this_thread::sleep_until(deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);

    EXPECT_FALSE(replayed_storage.exists("counter"));
}

TEST(AofPersistenceTest, NumericReplayPreservesAKeyRecreatedAfterExpiration) {
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
        AofWriter aof_writer(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, aof_writer, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline).applied);
        aof_writer.append(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        std::this_thread::sleep_until(deadline + std::chrono::milliseconds(25));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCR \"counter\""));
    }

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "1");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}

TEST(AofPersistenceTest, PersistAfterNumericMutationSurvivesTheOldDeadline) {
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
        AofWriter aof_writer(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, aof_writer, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", deadline).applied);
        aof_writer.append(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "PERSIST \"counter\""));
    }
    std::this_thread::sleep_until(deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_EQ(replayed_storage.ttl_milliseconds("counter"), -1);
}

TEST(AofPersistenceTest, ExtendingExpirationAfterNumericMutationSurvivesTheOldDeadline) {
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
        AofWriter aof_writer(log_file.path_string());

        ASSERT_TRUE(process_and_append(processor, aof_writer, "SET \"counter\" 10"));
        ASSERT_TRUE(storage.expire_at("counter", old_deadline).applied);
        aof_writer.append(RespCommandCodec::encode(
            {"PEXPIREAT", "counter", std::to_string(old_deadline_milliseconds)}));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "INCR \"counter\""));
        ASSERT_TRUE(process_and_append(processor, aof_writer, "EXPIRE \"counter\" 60"));
    }
    std::this_thread::sleep_until(old_deadline + std::chrono::milliseconds(25));

    StorageEngine replayed_storage;
    Executor replayed_executor(replayed_storage);
    AofReplayer(log_file.path_string()).replay(replayed_executor);

    ASSERT_TRUE(replayed_storage.get("counter").has_value());
    EXPECT_EQ(replayed_storage.get("counter")->bytes(), "11");
    EXPECT_GT(replayed_storage.ttl_milliseconds("counter"), 0);
}
