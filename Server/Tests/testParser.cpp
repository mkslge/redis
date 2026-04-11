//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Statements/SetStatement.h"
#include "Interpreter/Statements/Statement.h"
#include "Interpreter/Statements/StatementType.h"
#include "Interpreter/Tokens/Token.h"
#include "Interpreter/Tokens/TokenType.h"

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

TEST(ParserTest, TryParseSetBuildsSetStatementWithTypedValues) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        Token(TokenType::STRING, "name"),
        Token(TokenType::INT, 42)
    };

    auto parsed = Parser::try_parse_set<int>(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::SET);
    EXPECT_EQ(parsed->key(), "name");
    EXPECT_EQ(parsed->value(), 42);
}

TEST(ParserTest, TryParseSetRejectsWrongValueType) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        Token(TokenType::STRING, "name"),
        Token(TokenType::STRING, "forty-two")
    };

    auto parsed = Parser::try_parse_set<int>(tokens);

    EXPECT_EQ(parsed, nullptr);
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
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        Token(TokenType::STRING, "session-key"),
        Token(TokenType::INT, 30)
    };

    auto parsed = Parser::try_parse_expire(tokens);

    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->get_type(), StatementType::EXPIRE);
    EXPECT_EQ(parsed->key(), "session-key");
    EXPECT_EQ(parsed->expire_time(), 30);
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

    auto* set_statement = dynamic_cast<SetStatement<std::string>*>(parsed.get());
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
