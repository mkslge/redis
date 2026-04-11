//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>

#include <string>

#include "Interpreter/Executor/Executor.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"

namespace {
std::string capture_stdout(void (*fn)()) {
    testing::internal::CaptureStdout();
    fn();
    return testing::internal::GetCapturedStdout();
}
} // namespace

TEST(ExecutorTest, ExecuteGetPrintsQuotedStringKey) {
    auto output = capture_stdout([]() {
        Executor::execute_get(GetStatement("user"));
    });

    EXPECT_EQ(output, "GET key=\"user\"\n");
}

TEST(ExecutorTest, ExecuteGetPrintsStringKeyContainingDigits) {
    auto output = capture_stdout([]() {
        Executor::execute_get(GetStatement("42"));
    });

    EXPECT_EQ(output, "GET key=\"42\"\n");
}

TEST(ExecutorTest, ExecuteSetPrintsKeyAndValue) {
    auto output = capture_stdout([]() {
        Executor::execute_set(SetStatement<int>("count", 7));
    });

    EXPECT_EQ(output, "SET key=\"count\" value=7\n");
}

TEST(ExecutorTest, ExecuteSetQuotesStringValue) {
    auto output = capture_stdout([]() {
        Executor::execute_set(SetStatement<std::string>("x", "payload"));
    });

    EXPECT_EQ(output, "SET key=\"x\" value=\"payload\"\n");
}

TEST(ExecutorTest, ExecuteDeletePrintsKey) {
    auto output = capture_stdout([]() {
        Executor::execute_delete(DeleteStatement("3.5"));
    });

    EXPECT_EQ(output, "DELETE key=\"3.5\"\n");
}

TEST(ExecutorTest, ExecuteExistsPrintsKey) {
    auto output = capture_stdout([]() {
        Executor::execute_exists(ExistsStatement("k"));
    });

    EXPECT_EQ(output, "EXISTS key=\"k\"\n");
}

TEST(ExecutorTest, ExecuteExpirePrintsKeyAndTtl) {
    auto output = capture_stdout([]() {
        Executor::execute_expire(ExpireStatement("session", 30));
    });

    EXPECT_EQ(output, "EXPIRE key=\"session\" ttl=30\n");
}
