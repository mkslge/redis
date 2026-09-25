//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Protocol/ArgumentSplitter.h"
#include "Commands/Parser.h"
#include "Commands/ExecutionResult.h"
#include "Commands/Executor.h"

namespace {

std::optional<Command> parse_command(const std::string& command) {
    const auto arguments = ArgumentSplitter::split(command);
    if (!arguments.has_value()) {
        return std::nullopt;
    }

    ParseResult parsed = Parser::parse_request(*arguments);
    if (std::holds_alternative<ParseError>(parsed)) {
        return std::nullopt;
    }
    return std::get<Command>(std::move(parsed));
}

std::optional<ExecutionResult> execute_text(Executor& executor, const std::string& command) {
    const auto parsed = parse_command(command);
    if (!parsed.has_value()) return std::nullopt;
    return executor.execute(*parsed);
}
} // namespace

TEST(ExecutorTest, ExecuteGetReturnsEmptyPayloadForMissingKey) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_command("GET \"user\"");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result.payload));
}

TEST(ExecutorTest, ExecuteGetReturnsStoredValueForExistingKey) {
    StorageEngine storage;
    storage.set("42", Value("meaning"));
    Executor executor(storage);
    auto statement = parse_command("GET 42");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<Value>(result.payload));
    EXPECT_EQ(std::get<Value>(result.payload).bytes(), "meaning");
}

TEST(ExecutorTest, ExecuteSetStoresByteValue) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_command("SET \"count\" 7");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<Value>(result.payload));
    EXPECT_EQ(std::get<Value>(result.payload).bytes(), "7");
    ASSERT_TRUE(storage.get("count").has_value());
    EXPECT_EQ(storage.get("count")->bytes(), "7");
}

TEST(ExecutorTest, ExecuteSetQuotesStringValue) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_command("SET \"x\" \"payload\"");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<Value>(result.payload));
    EXPECT_EQ(std::get<Value>(result.payload).bytes(), "payload");
    ASSERT_TRUE(storage.get("x").has_value());
    EXPECT_EQ(storage.get("x")->bytes(), "payload");
}

TEST(ExecutorTest, ExecuteSetPreservesNumericSpelling) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_command("SET \"code\" 00123");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(storage.get("code").has_value());
    EXPECT_EQ(storage.get("code")->bytes(), "00123");
}

TEST(ExecutorTest, ExecuteDeleteReturnsDeletionResult) {
    StorageEngine storage;
    storage.set("3.5", Value("11"));
    Executor executor(storage);
    auto statement = parse_command("DEL 3.5");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<bool>(result.payload));
    EXPECT_TRUE(std::get<bool>(result.payload));
    EXPECT_FALSE(storage.exists("3.5"));
}

TEST(ExecutorTest, ExecuteExistsReturnsExistenceResult) {
    StorageEngine storage;
    storage.set("k", Value("v"));
    Executor executor(storage);
    auto statement = parse_command("EXISTS 'k'");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<bool>(result.payload));
    EXPECT_TRUE(std::get<bool>(result.payload));
}

TEST(ExecutorTest, ExecuteExpireReturnsApplyResult) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    Executor executor(storage);
    auto statement = parse_command("EXPIRE \"session\" 30");

    ASSERT_TRUE(statement.has_value());

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    ASSERT_TRUE(std::holds_alternative<bool>(result.payload));
    EXPECT_TRUE(std::get<bool>(result.payload));
    EXPECT_TRUE(storage.exists("session"));
}

TEST(ExecutorTest, ExecuteTtlAndPttlReturnRemainingLifetime) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    ASSERT_TRUE(storage.expire("session", std::chrono::seconds(30)));
    Executor executor(storage);

    auto ttl_statement = parse_command("TTL \"session\"");
    auto pttl_statement = parse_command("PTTL \"session\"");
    ASSERT_TRUE(ttl_statement.has_value());
    ASSERT_TRUE(pttl_statement.has_value());

    const auto ttl = std::get<std::int64_t>(executor.execute(*ttl_statement).payload);
    const auto pttl = std::get<std::int64_t>(executor.execute(*pttl_statement).payload);
    EXPECT_EQ(ttl, 30);
    EXPECT_GT(pttl, 29'000);
    EXPECT_LE(pttl, 30'000);
}

TEST(ExecutorTest, ExecuteTtlPreservesMissingAndPersistentSentinels) {
    StorageEngine storage;
    storage.set("permanent", Value("value"));
    Executor executor(storage);

    auto missing = parse_command("TTL \"missing\"");
    auto permanent = parse_command("PTTL \"permanent\"");

    EXPECT_EQ(std::get<std::int64_t>(executor.execute(*missing).payload), -2);
    EXPECT_EQ(std::get<std::int64_t>(executor.execute(*permanent).payload), -1);
}

TEST(ExecutorTest, ExecutePersistRemovesExpiration) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    ASSERT_TRUE(storage.expire("session", std::chrono::seconds(30)));
    Executor executor(storage);
    auto statement = parse_command("PERSIST \"session\"");

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(std::get<bool>(result.payload));
    EXPECT_EQ(storage.ttl_milliseconds("session"), -1);
}

TEST(ExecutorTest, NumericCommandsUpdateExistingIntegerValues) {
    struct Case {
        std::string command;
        std::int64_t expected;
    };

    for (const Case& test_case : std::vector<Case>{
             {"INCR \"counter\"", 11},
             {"DECR \"counter\"", 9},
             {"INCRBY \"counter\" 5", 15},
             {"DECRBY \"counter\" 3", 7}}) {
        SCOPED_TRACE(test_case.command);
        StorageEngine storage;
        storage.set("counter", Value("10"));
        Executor executor(storage);

        const auto result = execute_text(executor, test_case.command);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->success);
        EXPECT_TRUE(result->did_mutate);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(result->payload));
        EXPECT_EQ(std::get<std::int64_t>(result->payload), test_case.expected);
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), std::to_string(test_case.expected));
    }
}

TEST(ExecutorTest, NumericCommandsTreatMissingKeysAsZero) {
    struct Case {
        std::string command;
        std::int64_t expected;
    };

    for (const Case& test_case : std::vector<Case>{
             {"INCR \"counter\"", 1},
             {"DECR \"counter\"", -1},
             {"INCRBY \"counter\" 5", 5},
             {"DECRBY \"counter\" 3", -3}}) {
        SCOPED_TRACE(test_case.command);
        StorageEngine storage;
        Executor executor(storage);

        const auto result = execute_text(executor, test_case.command);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->success);
        EXPECT_TRUE(result->did_mutate);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(result->payload));
        EXPECT_EQ(std::get<std::int64_t>(result->payload), test_case.expected);
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), std::to_string(test_case.expected));
    }
}

TEST(ExecutorTest, ByCommandsAcceptNegativeAndZeroAmounts) {
    struct Case {
        std::string command;
        std::optional<Bytes> initial;
        std::int64_t expected;
    };

    for (const Case& test_case : std::vector<Case>{
             {"INCRBY \"counter\" -5", Bytes{"10"}, 5},
             {"DECRBY \"counter\" -5", Bytes{"10"}, 15},
             {"INCRBY \"counter\" 0", Bytes{"10"}, 10},
             {"INCRBY \"counter\" 0", std::nullopt, 0}}) {
        SCOPED_TRACE(test_case.command);
        StorageEngine storage;
        if (test_case.initial) storage.set("counter", Value(*test_case.initial));
        Executor executor(storage);

        const auto result = execute_text(executor, test_case.command);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->success);
        EXPECT_TRUE(result->did_mutate);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(result->payload));
        EXPECT_EQ(std::get<std::int64_t>(result->payload), test_case.expected);
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), std::to_string(test_case.expected));
    }
}

TEST(ExecutorTest, NumericCommandsRejectNonCanonicalStoredIntegersWithoutMutation) {
    const std::vector<Bytes> invalid_values{
        "", "1.0", "1e2", "12x", "+1", "01", "-0", " 1", "1 ", Bytes{"1\0", 2}};

    for (const Bytes& value : invalid_values) {
        SCOPED_TRACE(::testing::PrintToString(value));
        StorageEngine storage;
        storage.set("counter", Value(value));
        Executor executor(storage);

        const auto result = execute_text(executor, "INCR \"counter\"");

        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->success);
        EXPECT_FALSE(result->did_mutate);
        EXPECT_EQ(result->message, "value is not an integer or out of range");
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), value);
    }
}

TEST(ExecutorTest, ByCommandsRejectInvalidOperandsWithoutMutation) {
    const std::vector<Bytes> invalid_amounts{
        "", "1.0", "1e2", "12x", "+1", "01", "-0", " 1", "1 ",
        "9223372036854775808", "-9223372036854775809", Bytes{"1\0", 2}};

    for (const Bytes& amount : invalid_amounts) {
        SCOPED_TRACE(::testing::PrintToString(amount));
        StorageEngine storage;
        storage.set("counter", Value("10"));
        Executor executor(storage);
        const std::string command = "INCRBY \"counter\" \"" + amount + "\"";

        const auto result = execute_text(executor, command);

        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->success);
        EXPECT_FALSE(result->did_mutate);
        EXPECT_EQ(result->message, "value is not an integer or out of range");
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), "10");
    }
}

TEST(ExecutorTest, NumericCommandsRejectArithmeticOverflowWithoutMutation) {
    struct Case {
        Bytes initial;
        std::string command;
    };
    const std::string maximum = std::to_string(std::numeric_limits<std::int64_t>::max());
    const std::string minimum = std::to_string(std::numeric_limits<std::int64_t>::min());

    for (const Case& test_case : std::vector<Case>{
             {maximum, "INCR \"counter\""},
             {minimum, "DECR \"counter\""},
             {maximum, "INCRBY \"counter\" 1"},
             {minimum, "INCRBY \"counter\" -1"},
             {minimum, "DECRBY \"counter\" 1"},
             {maximum, "DECRBY \"counter\" -1"},
             {"0", "DECRBY \"counter\" -9223372036854775808"}}) {
        SCOPED_TRACE(test_case.command);
        StorageEngine storage;
        storage.set("counter", Value(test_case.initial));
        Executor executor(storage);

        const auto result = execute_text(executor, test_case.command);

        ASSERT_TRUE(result.has_value());
        EXPECT_FALSE(result->success);
        EXPECT_FALSE(result->did_mutate);
        EXPECT_EQ(result->message, "increment or decrement would overflow");
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), test_case.initial);
    }
}

TEST(ExecutorTest, NumericCommandsAllowResultsAtSigned64BitBounds) {
    struct Case {
        std::optional<Bytes> initial;
        std::string command;
        std::int64_t expected;
    };

    for (const Case& test_case : std::vector<Case>{
             {Bytes{"9223372036854775806"}, "INCR \"counter\"", std::numeric_limits<std::int64_t>::max()},
             {Bytes{"-9223372036854775807"}, "DECR \"counter\"", std::numeric_limits<std::int64_t>::min()},
             {std::nullopt, "INCRBY \"counter\" -9223372036854775808", std::numeric_limits<std::int64_t>::min()}}) {
        SCOPED_TRACE(test_case.command);
        StorageEngine storage;
        if (test_case.initial) storage.set("counter", Value(*test_case.initial));
        Executor executor(storage);

        const auto result = execute_text(executor, test_case.command);

        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->success);
        ASSERT_TRUE(std::holds_alternative<std::int64_t>(result->payload));
        EXPECT_EQ(std::get<std::int64_t>(result->payload), test_case.expected);
        ASSERT_TRUE(storage.get("counter").has_value());
        EXPECT_EQ(storage.get("counter")->bytes(), std::to_string(test_case.expected));
    }
}

TEST(ExecutorTest, NumericCommandsPreserveExistingExpiration) {
    for (const std::string command : {
             "INCR \"counter\"", "DECR \"counter\"",
             "INCRBY \"counter\" 5", "DECRBY \"counter\" 3"}) {
        SCOPED_TRACE(command);
        StorageEngine storage;
        storage.set("counter", Value("10"));
        ASSERT_TRUE(storage.expire("counter", std::chrono::seconds(30)));
        const std::int64_t before = storage.ttl_milliseconds("counter");
        Executor executor(storage);

        const auto result = execute_text(executor, command);

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->success);
        const std::int64_t after = storage.ttl_milliseconds("counter");
        EXPECT_GT(after, 0);
        EXPECT_LE(after, before);
        EXPECT_TRUE(storage.possibly_expired().contains("counter"));
    }
}

TEST(ExecutorTest, FailedNumericCommandLeavesExpirationIntact) {
    StorageEngine storage;
    storage.set("counter", Value("not-an-integer"));
    ASSERT_TRUE(storage.expire("counter", std::chrono::seconds(30)));
    Executor executor(storage);

    const auto result = execute_text(executor, "INCR \"counter\"");

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->success);
    ASSERT_TRUE(storage.get("counter").has_value());
    EXPECT_EQ(storage.get("counter")->bytes(), "not-an-integer");
    EXPECT_GT(storage.ttl_milliseconds("counter"), 0);
    EXPECT_TRUE(storage.possibly_expired().contains("counter"));
}

TEST(ExecutorTest, NumericCommandsTreatExpiredKeysAsMissingAndRecreateThemPersistently) {
    StorageEngine storage;
    storage.set("counter", Value("10"));
    ASSERT_TRUE(storage.expire_at("counter", StorageEngine::Clock::now() - std::chrono::seconds(1)).applied);
    Executor executor(storage);

    const auto result = execute_text(executor, "INCR \"counter\"");

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->success);
    ASSERT_TRUE(storage.get("counter").has_value());
    EXPECT_EQ(storage.get("counter")->bytes(), "1");
    EXPECT_EQ(storage.ttl_milliseconds("counter"), -1);
}

TEST(ExecutorTest, ConcurrentIncrementsDoNotLoseUpdates) {
    constexpr int kThreadCount = 8;
    constexpr int kIncrementsPerThread = 1'000;
    StorageEngine storage;
    const auto command = parse_command("INCR \"counter\"");
    ASSERT_TRUE(command.has_value());
    std::barrier start_line(kThreadCount);
    std::atomic<bool> all_succeeded{true};
    std::vector<std::thread> threads;

    for (int thread = 0; thread < kThreadCount; ++thread) {
        threads.emplace_back([&] {
            Executor executor(storage);
            start_line.arrive_and_wait();
            for (int increment = 0; increment < kIncrementsPerThread; ++increment) {
                const ExecutionResult result = executor.execute(*command);
                if (!result.success) all_succeeded = false;
            }
        });
    }
    for (auto& thread : threads) thread.join();

    EXPECT_TRUE(all_succeeded);
    ASSERT_TRUE(storage.get("counter").has_value());
    EXPECT_EQ(storage.get("counter")->bytes(),
              std::to_string(kThreadCount * kIncrementsPerThread));
}
