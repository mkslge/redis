//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>

#include <string>

#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"

namespace {
template <typename Fn>
std::string capture_stdout(Fn&& fn) {
    testing::internal::CaptureStdout();
    fn();
    return testing::internal::GetCapturedStdout();
}
} // namespace

TEST(ExecutorTest, ExecuteGetPrintsQuotedStringKey) {
    StorageEngine storage;
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_get(GetStatement("user"));
    });

    EXPECT_EQ(output, "GET key=\"user\"\n");
}

TEST(ExecutorTest, ExecuteGetPrintsStringKeyContainingDigits) {
    StorageEngine storage;
    storage.set("42", Value("meaning"));
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_get(GetStatement("42"));
    });

    EXPECT_EQ(output, "GET key=\"42\" value=\"meaning\"\n");
}

TEST(ExecutorTest, ExecuteSetPrintsKeyAndValue) {
    StorageEngine storage;
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_set(SetStatement<int>("count", 7));
    });

    EXPECT_EQ(output, "SET key=\"count\" value=7\n");
    ASSERT_TRUE(storage.get("count").has_value());
    EXPECT_EQ(storage.get("count")->get<int>(), 7);
}

TEST(ExecutorTest, ExecuteSetQuotesStringValue) {
    StorageEngine storage;
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_set(SetStatement<std::string>("x", "payload"));
    });

    EXPECT_EQ(output, "SET key=\"x\" value=\"payload\"\n");
    ASSERT_TRUE(storage.get("x").has_value());
    EXPECT_EQ(storage.get("x")->get<std::string>(), "payload");
}

TEST(ExecutorTest, ExecuteDeletePrintsKey) {
    StorageEngine storage;
    storage.set("3.5", Value(11));
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_delete(DeleteStatement("3.5"));
    });

    EXPECT_EQ(output, "DELETE key=\"3.5\" deleted=true\n");
    EXPECT_FALSE(storage.exists("3.5"));
}

TEST(ExecutorTest, ExecuteExistsPrintsKey) {
    StorageEngine storage;
    storage.set("k", Value('v'));
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_exists(ExistsStatement("k"));
    });

    EXPECT_EQ(output, "EXISTS key=\"k\" exists=true\n");
}

TEST(ExecutorTest, ExecuteExpirePrintsKeyAndTtl) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    Executor executor(storage);

    auto output = capture_stdout([&]() {
        executor.execute_expire(ExpireStatement("session", 30));
    });

    EXPECT_EQ(output, "EXPIRE key=\"session\" ttl=30 applied=true\n");
    EXPECT_TRUE(storage.exists("session"));
}
