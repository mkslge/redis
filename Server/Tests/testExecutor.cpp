//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"
#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Runtime/ExecutionResult.h"

namespace {

std::unique_ptr<Statement> parse_statement(const std::string& command) {
    auto tokens = Tokenizer::tokenize(command);
    if (!tokens.has_value()) {
        return nullptr;
    }

    std::vector<Token> parsed_tokens = tokens.value();
    return Parser::parse(parsed_tokens);
}
} // namespace

TEST(ExecutorTest, ExecuteGetReturnsEmptyPayloadForMissingKey) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_statement("GET \"user\"");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "GET");
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result.payload));
}

TEST(ExecutorTest, ExecuteGetReturnsStoredValueForExistingKey) {
    StorageEngine storage;
    storage.set("42", Value("meaning"));
    Executor executor(storage);
    auto statement = parse_statement("GET 42");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "GET");
    ASSERT_TRUE(std::holds_alternative<Value>(result.payload));
    EXPECT_EQ(std::get<Value>(result.payload).get<std::string>(), "meaning");
}

TEST(ExecutorTest, ExecuteSetStoresTypedValue) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_statement("SET \"count\" 7");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "SET");
    ASSERT_TRUE(std::holds_alternative<Value>(result.payload));
    EXPECT_EQ(std::get<Value>(result.payload).get<int>(), 7);
    ASSERT_TRUE(storage.get("count").has_value());
    EXPECT_EQ(storage.get("count")->get<int>(), 7);
}

TEST(ExecutorTest, ExecuteSetQuotesStringValue) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_statement("SET \"x\" \"payload\"");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "SET");
    ASSERT_TRUE(std::holds_alternative<Value>(result.payload));
    EXPECT_EQ(std::get<Value>(result.payload).get<std::string>(), "payload");
    ASSERT_TRUE(storage.get("x").has_value());
    EXPECT_EQ(storage.get("x")->get<std::string>(), "payload");
}

TEST(ExecutorTest, ExecuteDeleteReturnsDeletionResult) {
    StorageEngine storage;
    storage.set("3.5", Value(11));
    Executor executor(storage);
    auto statement = parse_statement("DEL 3.5");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "DELETE");
    ASSERT_TRUE(std::holds_alternative<bool>(result.payload));
    EXPECT_TRUE(std::get<bool>(result.payload));
    EXPECT_FALSE(storage.exists("3.5"));
}

TEST(ExecutorTest, ExecuteExistsReturnsExistenceResult) {
    StorageEngine storage;
    storage.set("k", Value('v'));
    Executor executor(storage);
    auto statement = parse_statement("EXISTS 'k'");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "EXISTS");
    ASSERT_TRUE(std::holds_alternative<bool>(result.payload));
    EXPECT_TRUE(std::get<bool>(result.payload));
}

TEST(ExecutorTest, ExecuteExpireReturnsApplyResult) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    Executor executor(storage);
    auto statement = parse_statement("EXPIRE \"session\" 30");

    ASSERT_NE(statement, nullptr);

    const ExecutionResult result = executor.execute(*statement);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "EXPIRE");
    ASSERT_TRUE(std::holds_alternative<bool>(result.payload));
    EXPECT_TRUE(std::get<bool>(result.payload));
    EXPECT_TRUE(storage.exists("session"));
}
