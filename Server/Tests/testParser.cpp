//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "DeleteStatement.h"
#include "ExistsStatement.h"
#include "ExpireStatement.h"
#include "GetStatement.h"
#include "SetStatement.h"
#include "Statement.h"
#include "StatementType.h"
#include "Parser.h"
#include "Token.h"
#include "TokenType.h"

TEST(ParserTest, TryParseGetAcceptsPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::GET),
        Token(TokenType::STRING, "session-key")
    };

    auto parsed = Parser::try_parse_get(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::GET);
    EXPECT_EQ(parsed->key(), "session-key");
}

TEST(ParserTest, TryParseGetRejectsNonPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::GET),
        Token(TokenType::SET)
    };

    auto parsed = Parser::try_parse_get(tokens);

    EXPECT_EQ(parsed, nullptr);
}

TEST(ParserTest, TryParseSetBuildsSetStatementWithByteValue) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        Token(TokenType::STRING, "name"),
        Token(TokenType::INT, 42)
    };

    auto parsed = Parser::try_parse_set(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::SET);
    EXPECT_EQ(parsed->key(), "name");
    EXPECT_EQ(parsed->value(), "42");
}

TEST(ParserTest, TryParseSetAcceptsStringValue) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        Token(TokenType::STRING, "name"),
        Token(TokenType::STRING, "forty-two")
    };

    auto parsed = Parser::try_parse_set(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->value(), "forty-two");
}

TEST(ParserTest, TryParseDeleteAcceptsPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::DEL),
        Token(TokenType::INT, 7)
    };

    auto parsed = Parser::try_parse_del(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::DELETE);
    EXPECT_EQ(parsed->key(), "7");
}

TEST(ParserTest, TryParseExistsAcceptsPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::EXISTS),
        Token(TokenType::CHAR, 'k')
    };

    auto parsed = Parser::try_parse_exists(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::EXISTS);
    EXPECT_EQ(parsed->key(), "k");
}

TEST(ParserTest, TryParseExpireParsesKeyAndTtl) {
    const auto before = ExpireStatement::Clock::now();
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        Token(TokenType::STRING, "session-key"),
        Token(TokenType::INT, 30)
    };

    auto parsed = Parser::try_parse_expire(tokens);
    const auto after = ExpireStatement::Clock::now();

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::EXPIRE);
    EXPECT_EQ(parsed->key(), "session-key");
    EXPECT_GE(parsed->expires_at(), before + std::chrono::seconds(30));
    EXPECT_LE(parsed->expires_at(), after + std::chrono::seconds(30));
}

TEST(ParserTest, ParseArgumentsReadsAbsoluteExpirationFromAof) {
    constexpr std::int64_t deadline_milliseconds = 4'102'444'800'000;

    auto statement = Parser::parse_arguments(
        {"PEXPIREAT", "session-key", std::to_string(deadline_milliseconds)});

    ASSERT_NE(statement, nullptr);
    auto* expiration = dynamic_cast<ExpireStatement*>(statement.get());
    ASSERT_NE(expiration, nullptr);
    EXPECT_EQ(expiration->key(), "session-key");
    EXPECT_EQ(expiration->expires_at_unix_milliseconds(), deadline_milliseconds);
}

TEST(ParserTest, ParseArgumentsRejectsRelativeExpireInAof) {
    EXPECT_EQ(Parser::parse_arguments({"EXPIRE", "session-key", "30"}), nullptr);
}

TEST(ParserTest, TryParseExpireRejectsNonIntegerTtl) {
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        Token(TokenType::STRING, "session-key"),
        Token(TokenType::DOUBLE, 30.5)
    };

    auto parsed = Parser::try_parse_expire(tokens);

    EXPECT_EQ(parsed, nullptr);
}

TEST(ParserTest, ParseDispatchesSetStatements) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        Token(TokenType::STRING, "user"),
        Token(TokenType::STRING, "mark")
    };

    auto parsed = Parser::parse(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::SET);

    auto* set_statement = dynamic_cast<SetStatement*>(parsed.get());
    ASSERT_NE(set_statement, nullptr);
    EXPECT_EQ(set_statement->key(), "user");
    EXPECT_EQ(set_statement->value(), "mark");
}

TEST(ParserTest, ParseRejectsMalformedCommands) {
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        Token(TokenType::STRING, "session-key")
    };

    auto parsed = Parser::parse(tokens);

    EXPECT_EQ(parsed, nullptr);
}
