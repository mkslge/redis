#include "CommandProcessor.h"
#include "Executor.h"
#include "StorageEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

static_assert(!std::is_default_constructible_v<CommandProcessResult>);

TEST(CommandProcessorTest, ValidCommandProducesProcessedCommand) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const CommandProcessResult result = processor.process("GET \"missing\"");

    ASSERT_TRUE(result.is_success());
    EXPECT_TRUE(std::holds_alternative<GetCommand>(result.processed_command().command));
    EXPECT_THROW(result.error_message(), std::bad_variant_access);
}

TEST(CommandProcessorTest, InvalidInputProducesError) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const CommandProcessResult result = processor.process("UNKNOWN");

    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error_message(), "unknown command");
    EXPECT_THROW(result.processed_command(), std::bad_variant_access);
}

TEST(CommandProcessorTest, InvalidCommandShapeProducesParseError) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const CommandProcessResult result = processor.process("GET");

    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error_message(), "wrong number of arguments for 'get' command");
}

TEST(CommandProcessorTest, SuccessfulNumericCommandsAreLoggedAsMutations) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    for (const std::string command : {
             "INCR \"counter\"", "DECR \"counter\"",
             "INCRBY \"counter\" 5", "DECRBY \"counter\" 5",
             "INCRBY \"counter\" 0"}) {
        SCOPED_TRACE(command);
        const CommandProcessResult result = processor.process(command);

        ASSERT_TRUE(result.is_success());
        EXPECT_TRUE(result.processed_command().execution_result.success);
        EXPECT_TRUE(result.processed_command().execution_result.did_mutate);
        EXPECT_TRUE(result.processed_command().mutating_command);
        EXPECT_TRUE(result.processed_command().should_log);
        EXPECT_FALSE(result.processed_command().aof_record.empty());
    }
}

TEST(CommandProcessorTest, FailedNumericCommandsDoNotProduceAofRecords) {
    StorageEngine storage;
    storage.set("not-integer", Value("value"));
    storage.set("maximum", Value("9223372036854775807"));
    Executor executor(storage);
    CommandProcessor processor(executor);

    for (const auto& [command, expected_message] :
         std::vector<std::pair<std::string, std::string>>{
             {"INCR \"not-integer\"", "value is not an integer or out of range"},
             {"INCR \"maximum\"", "increment or decrement would overflow"}}) {
        SCOPED_TRACE(command);
        const CommandProcessResult result = processor.process(command);

        ASSERT_TRUE(result.is_success());
        EXPECT_FALSE(result.processed_command().execution_result.success);
        EXPECT_FALSE(result.processed_command().execution_result.did_mutate);
        EXPECT_EQ(result.processed_command().execution_result.message, expected_message);
        EXPECT_TRUE(result.processed_command().mutating_command);
        EXPECT_FALSE(result.processed_command().should_log);
        EXPECT_TRUE(result.processed_command().aof_record.empty());
    }
}

TEST(CommandProcessorTest, UnquotedOutOfRangeAmountIsAnExecutionError) {
    StorageEngine storage;
    storage.set("counter", Value("10"));
    Executor executor(storage);
    CommandProcessor processor(executor);

    const auto result = processor.process("INCRBY \"counter\" 9223372036854775808");

    ASSERT_TRUE(result.is_success());
    EXPECT_FALSE(result.processed_command().execution_result.success);
    EXPECT_EQ(result.processed_command().execution_result.message,
              "value is not an integer or out of range");
    EXPECT_FALSE(result.processed_command().should_log);
    ASSERT_TRUE(storage.get("counter").has_value());
    EXPECT_EQ(storage.get("counter")->bytes(), "10");
}

TEST(CommandProcessorTest, GetUsesNumericLookingKeyBytesVerbatim) {
    StorageEngine storage;
    storage.set("007", Value("bond"));
    Executor executor(storage);
    CommandProcessor processor(executor);

    const auto result = processor.process("GET 007");

    ASSERT_TRUE(result.is_success());
    EXPECT_EQ(std::get<GetCommand>(result.processed_command().command).key, "007");
    EXPECT_EQ(std::get<Value>(result.processed_command().execution_result.payload), Value("bond"));
}

TEST(CommandProcessorTest, SetStoresNumericLookingKeyBytesVerbatim) {
    for (const std::string key : {"1.50", "1e3"}) {
        SCOPED_TRACE(key);
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor processor(executor);

        const auto result = processor.process("SET " + key + " \"x\"");

        EXPECT_TRUE(result.is_success());
        if (result.is_success()) {
            EXPECT_EQ(std::get<SetCommand>(result.processed_command().command).key, key);
        }
        EXPECT_EQ(storage.get(key), std::optional<Value>(Value("x")));
    }
}

TEST(CommandProcessorTest, CommandNameAsArgumentIsTreatedAsBytes) {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    const auto result = processor.process("SET \"k\" get");

    ASSERT_TRUE(result.is_success());
    ASSERT_TRUE(storage.get("k").has_value());
    EXPECT_EQ(storage.get("k")->bytes(), "get");
}

TEST(CommandProcessorTest, ExpireAcceptsQuotedSeconds) {
    StorageEngine storage;
    storage.set("k", Value("v"));
    Executor executor(storage);
    CommandProcessor processor(executor);

    const auto before = ExpireCommand::Clock::now();
    const auto result = processor.process("EXPIRE \"k\" \"30\"");

    ASSERT_TRUE(result.is_success());
    const auto& command = std::get<ExpireCommand>(result.processed_command().command);
    EXPECT_EQ(command.key, "k");
    EXPECT_GE(command.expires_at, before + std::chrono::seconds(30));
    EXPECT_TRUE(result.processed_command().execution_result.success);
    EXPECT_TRUE(result.processed_command().execution_result.did_mutate);
}
