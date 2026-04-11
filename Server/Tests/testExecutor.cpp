//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"
#include "Interpreter/Runtime/Executor.h"

namespace {
template <typename Fn>
std::string capture_stdout(Fn&& fn) {
    testing::internal::CaptureStdout();
    fn();
    return testing::internal::GetCapturedStdout();
}

std::unique_ptr<Statement> parse_statement(const std::string& command) {
    auto tokens = Tokenizer::tokenize(command);
    if (!tokens.has_value()) {
        return nullptr;
    }

    std::vector<Token> parsed_tokens = tokens.value();
    return Parser::parse(parsed_tokens);
}
} // namespace

TEST(ExecutorTest, ExecuteGetPrintsQuotedStringKey) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_statement("GET \"user\"");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "GET key=\"user\"\n");
}

TEST(ExecutorTest, ProcessCommandPrintsGetValueForExistingKey) {
    StorageEngine storage;
    storage.set("42", Value("meaning"));
    Executor executor(storage);
    auto statement = parse_statement("GET 42");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "GET key=\"42\" value=\"meaning\"\n");
}

TEST(ExecutorTest, ExecuteSetStoresTypedValue) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_statement("SET \"count\" 7");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "SET key=\"count\" value=7\n");
    ASSERT_TRUE(storage.get("count").has_value());
    EXPECT_EQ(storage.get("count")->get<int>(), 7);
}

TEST(ExecutorTest, ExecuteSetQuotesStringValue) {
    StorageEngine storage;
    Executor executor(storage);
    auto statement = parse_statement("SET \"x\" \"payload\"");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "SET key=\"x\" value=\"payload\"\n");
    ASSERT_TRUE(storage.get("x").has_value());
    EXPECT_EQ(storage.get("x")->get<std::string>(), "payload");
}

TEST(ExecutorTest, ExecuteDeletePrintsKey) {
    StorageEngine storage;
    storage.set("3.5", Value(11));
    Executor executor(storage);
    auto statement = parse_statement("DEL 3.5");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "DELETE key=\"3.5\" deleted=true\n");
    EXPECT_FALSE(storage.exists("3.5"));
}

TEST(ExecutorTest, ExecuteExistsPrintsKey) {
    StorageEngine storage;
    storage.set("k", Value('v'));
    Executor executor(storage);
    auto statement = parse_statement("EXISTS 'k'");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "EXISTS key=\"k\" exists=true\n");
}

TEST(ExecutorTest, ExecuteExpirePrintsKeyAndTtl) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    Executor executor(storage);
    auto statement = parse_statement("EXPIRE \"session\" 30");

    ASSERT_NE(statement, nullptr);

    auto output = capture_stdout([&]() {
        executor.execute(*statement);
    });

    EXPECT_EQ(output, "EXPIRE key=\"session\" ttl=30 applied=true\n");
    EXPECT_TRUE(storage.exists("session"));
}
