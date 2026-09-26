#include "Network/ClientSession.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>

namespace {
using Clock = ClientSession::Clock;
const Clock::time_point kStart{};
} // namespace

TEST(ClientSessionTest, NextLineWaitsForNewline) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("GET k", kStart));
    EXPECT_EQ(session.next_line(), std::nullopt);
    ASSERT_TRUE(session.append_input("ey\n", kStart));
    EXPECT_EQ(session.next_line(), "GET key");
    EXPECT_EQ(session.next_line(), std::nullopt);
}

TEST(ClientSessionTest, NextLineSplitsPipelinedInputAndStripsCarriageReturn) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("SET k v\r\nGET k\n\nPART", kStart));
    EXPECT_EQ(session.next_line(), "SET k v");
    EXPECT_EQ(session.next_line(), "GET k");
    EXPECT_EQ(session.next_line(), "");
    EXPECT_EQ(session.next_line(), std::nullopt);
}

TEST(ClientSessionTest, DiscardInputDropsUnreadLines) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("QUIT\nGET k\n", kStart));
    EXPECT_EQ(session.next_line(), "QUIT");
    session.discard_input();
    EXPECT_EQ(session.next_line(), std::nullopt);
}

TEST(ClientSessionTest, AppendInputRejectsOversizedBuffer) {
    ClientSession session;
    EXPECT_TRUE(session.append_input(std::string(ClientSession::kMaxInputBuffer, 'x'), kStart));
    EXPECT_FALSE(session.append_input("y", kStart));
}

TEST(ClientSessionTest, OutputIsConsumedInPartialWrites) {
    ClientSession session;
    EXPECT_FALSE(session.has_pending_output());
    ASSERT_TRUE(session.queue_response("GET\n", kStart));
    ASSERT_TRUE(session.queue_response("SET value=\"v\"\n", kStart));
    EXPECT_EQ(session.pending_output(), "GET\nSET value=\"v\"\n");

    session.consume_output(4, kStart);
    EXPECT_EQ(session.pending_output(), "SET value=\"v\"\n");
    ASSERT_TRUE(session.queue_response("BYE\n", kStart));
    EXPECT_EQ(session.pending_output(), "SET value=\"v\"\nBYE\n");

    session.consume_output(session.pending_output().size(), kStart);
    EXPECT_FALSE(session.has_pending_output());
    EXPECT_EQ(session.pending_output(), "");
}

TEST(ClientSessionTest, QueueResponseRejectsOutputOverLimit) {
    ClientSession session;
    ASSERT_TRUE(session.queue_response(std::string(ClientSession::kMaxOutputBuffer - 1, 'x'), kStart));
    EXPECT_TRUE(session.queue_response("y", kStart));
    EXPECT_FALSE(session.queue_response("z", kStart));
    session.consume_output(10, kStart);
    EXPECT_TRUE(session.queue_response("z", kStart));
}

TEST(ClientSessionTest, CloseAfterWriteIsSticky) {
    ClientSession session;
    EXPECT_FALSE(session.closing());
    session.close_after_write();
    EXPECT_TRUE(session.closing());
}

namespace {
const ClientSession::Timeouts kTimeouts{std::chrono::seconds(10), std::chrono::seconds(30)};

Clock::time_point at(const int seconds) {
    return kStart + std::chrono::seconds(seconds);
}
} // namespace

TEST(ClientSessionTimeoutTest, IdleClientNeverTimesOut) {
    ClientSession session;
    EXPECT_EQ(session.timed_out(at(24 * 60 * 60), kTimeouts), std::nullopt);

    ASSERT_TRUE(session.append_input("GET k\n", at(0)));
    ASSERT_TRUE(session.next_line().has_value());
    EXPECT_EQ(session.timed_out(at(24 * 60 * 60), kTimeouts), std::nullopt);
}

TEST(ClientSessionTimeoutTest, UnfinishedLineTimesOut) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("SET k", at(0)));

    EXPECT_EQ(session.timed_out(at(9), kTimeouts), std::nullopt);
    EXPECT_EQ(session.timed_out(at(10), kTimeouts), "request line not finished in time");
}

TEST(ClientSessionTimeoutTest, TricklingBytesDoesNotExtendTheLineDeadline) {
    ClientSession session;
    for (int second = 0; second < 10; ++second) {
        ASSERT_TRUE(session.append_input("x", at(second)));
    }
    EXPECT_TRUE(session.timed_out(at(10), kTimeouts).has_value());
}

TEST(ClientSessionTimeoutTest, FinishingTheLineClearsTheDeadline) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("SET k", at(0)));
    ASSERT_TRUE(session.append_input(" v\n", at(9)));
    ASSERT_TRUE(session.next_line().has_value());

    EXPECT_EQ(session.timed_out(at(60), kTimeouts), std::nullopt);
}

TEST(ClientSessionTimeoutTest, NextPartialLineGetsItsOwnDeadline) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("SET k", at(0)));
    ASSERT_TRUE(session.append_input(" v\nGET", at(8)));
    ASSERT_TRUE(session.next_line().has_value());

    EXPECT_EQ(session.timed_out(at(17), kTimeouts), std::nullopt);
    EXPECT_TRUE(session.timed_out(at(18), kTimeouts).has_value());
}

TEST(ClientSessionTimeoutTest, StalledOutputTimesOut) {
    ClientSession session;
    ASSERT_TRUE(session.queue_response("GET value=\"v\"\n", at(0)));
    // More output queued behind a stalled client is not progress.
    ASSERT_TRUE(session.queue_response("GET value=\"v\"\n", at(20)));

    EXPECT_EQ(session.timed_out(at(29), kTimeouts), std::nullopt);
    EXPECT_EQ(session.timed_out(at(30), kTimeouts), "no response bytes accepted in time");
}

TEST(ClientSessionTimeoutTest, SlowButSteadyOutputNeverTimesOut) {
    ClientSession session;
    ASSERT_TRUE(session.queue_response(std::string(100, 'x'), at(0)));
    for (int second = 20; second <= 80; second += 20) {
        session.consume_output(10, at(second));
        EXPECT_EQ(session.timed_out(at(second + 29), kTimeouts), std::nullopt);
    }

    session.consume_output(session.pending_output().size(), at(100));
    EXPECT_EQ(session.timed_out(at(1000), kTimeouts), std::nullopt);
}

TEST(ClientSessionTimeoutTest, DiscardingInputClearsTheLineDeadline) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("QUIT\nSET k", at(0)));
    ASSERT_TRUE(session.next_line().has_value());
    session.discard_input();

    EXPECT_EQ(session.timed_out(at(60), kTimeouts), std::nullopt);
}
