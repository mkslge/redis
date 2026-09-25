#include "Network/ClientSession.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

TEST(ClientSessionTest, NextLineWaitsForNewline) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("GET k"));
    EXPECT_EQ(session.next_line(), std::nullopt);
    ASSERT_TRUE(session.append_input("ey\n"));
    EXPECT_EQ(session.next_line(), "GET key");
    EXPECT_EQ(session.next_line(), std::nullopt);
}

TEST(ClientSessionTest, NextLineSplitsPipelinedInputAndStripsCarriageReturn) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("SET k v\r\nGET k\n\nPART"));
    EXPECT_EQ(session.next_line(), "SET k v");
    EXPECT_EQ(session.next_line(), "GET k");
    EXPECT_EQ(session.next_line(), "");
    EXPECT_EQ(session.next_line(), std::nullopt);
}

TEST(ClientSessionTest, DiscardInputDropsUnreadLines) {
    ClientSession session;
    ASSERT_TRUE(session.append_input("QUIT\nGET k\n"));
    EXPECT_EQ(session.next_line(), "QUIT");
    session.discard_input();
    EXPECT_EQ(session.next_line(), std::nullopt);
}

TEST(ClientSessionTest, AppendInputRejectsOversizedBuffer) {
    ClientSession session;
    EXPECT_TRUE(session.append_input(std::string(ClientSession::kMaxInputBuffer, 'x')));
    EXPECT_FALSE(session.append_input("y"));
}

TEST(ClientSessionTest, OutputIsConsumedInPartialWrites) {
    ClientSession session;
    EXPECT_FALSE(session.has_pending_output());
    ASSERT_TRUE(session.queue_response("GET\n"));
    ASSERT_TRUE(session.queue_response("SET value=\"v\"\n"));
    EXPECT_EQ(session.pending_output(), "GET\nSET value=\"v\"\n");

    session.consume_output(4);
    EXPECT_EQ(session.pending_output(), "SET value=\"v\"\n");
    ASSERT_TRUE(session.queue_response("BYE\n"));
    EXPECT_EQ(session.pending_output(), "SET value=\"v\"\nBYE\n");

    session.consume_output(session.pending_output().size());
    EXPECT_FALSE(session.has_pending_output());
    EXPECT_EQ(session.pending_output(), "");
}

TEST(ClientSessionTest, QueueResponseRejectsOutputOverLimit) {
    ClientSession session;
    ASSERT_TRUE(session.queue_response(std::string(ClientSession::kMaxOutputBuffer - 1, 'x')));
    EXPECT_TRUE(session.queue_response("y"));
    EXPECT_FALSE(session.queue_response("z"));
    session.consume_output(10);
    EXPECT_TRUE(session.queue_response("z"));
}

TEST(ClientSessionTest, CloseAfterWriteIsSticky) {
    ClientSession session;
    EXPECT_FALSE(session.closing());
    session.close_after_write();
    EXPECT_TRUE(session.closing());
}
