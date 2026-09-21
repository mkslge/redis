//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Parser.h"
#include "Tokenizer.h"
#include "ExecutionResult.h"
#include "Executor.h"

namespace {

std::optional<Command> parse_command(const std::string& command) {
    auto tokens = Tokenizer::tokenize(command);
    if (!tokens.has_value()) {
        return std::nullopt;
    }

    std::vector<Token> parsed_tokens = tokens.value();
    return Parser::parse(parsed_tokens);
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
