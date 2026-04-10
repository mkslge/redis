//
// Created by Mark on 4/10/26.
//

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../Interpreter/DeleteStatement.h"
#include "../Interpreter/ExistsStatement.h"
#include "../Interpreter/ExpireStatement.h"
#include "../Interpreter/GetStatement.h"
#include "../Interpreter/Parser.h"
#include "../Interpreter/PrimToken.h"
#include "../Interpreter/SetStatement.h"
#include "../Interpreter/Statement.h"
#include "../Interpreter/StatementType.h"
#include "../Interpreter/Token.h"
#include "../Interpreter/TokenType.h"

TEST(ParserTest, TryParseGetAcceptsPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::GET),
        PrimToken<std::string>(TokenType::STRING, "session-key")
    };

    auto parsed = Parser::try_parse_get<std::string>(tokens);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_NE(parsed.value(), nullptr);
    EXPECT_EQ(parsed.value()->get_type(), StatementType::GET);
    EXPECT_EQ(parsed.value()->key(), "session-key");
}

TEST(ParserTest, TryParseGetRejectsNonPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::GET),
        Token(TokenType::SET)
    };

    auto parsed = Parser::try_parse_get<std::string>(tokens);

    EXPECT_FALSE(parsed.has_value());
}

TEST(ParserTest, TryParseSetBuildsSetStatementWithTypedValues) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        PrimToken<std::string>(TokenType::STRING, "name"),
        PrimToken<int>(TokenType::INT, 42)
    };

    auto parsed = Parser::try_parse_set<std::string, int>(tokens);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_NE(parsed.value(), nullptr);
    EXPECT_EQ(parsed.value()->get_type(), StatementType::SET);
    EXPECT_EQ(parsed.value()->key(), "name");
    EXPECT_EQ(parsed.value()->value(), 42);
}

TEST(ParserTest, TryParseSetRejectsWrongValueType) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        PrimToken<std::string>(TokenType::STRING, "name"),
        PrimToken<std::string>(TokenType::STRING, "forty-two")
    };

    auto parsed = Parser::try_parse_set<std::string, int>(tokens);

    EXPECT_FALSE(parsed.has_value());
}

TEST(ParserTest, TryParseDeleteAcceptsPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::DEL),
        PrimToken<int>(TokenType::INT, 7)
    };

    auto parsed = Parser::try_parse_del<int>(tokens);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_NE(parsed.value(), nullptr);
    EXPECT_EQ(parsed.value()->get_type(), StatementType::DELETE);
    EXPECT_EQ(parsed.value()->key(), 7);
}

TEST(ParserTest, TryParseExistsAcceptsPrimitiveKey) {
    std::vector<Token> tokens = {
        Token(TokenType::EXISTS),
        PrimToken<char>(TokenType::CHAR, 'k')
    };

    auto parsed = Parser::try_parse_exists<char>(tokens);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_NE(parsed.value(), nullptr);
    EXPECT_EQ(parsed.value()->get_type(), StatementType::EXISTS);
    EXPECT_EQ(parsed.value()->key(), 'k');
}

TEST(ParserTest, TryParseExpireParsesKeyAndTtl) {
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        PrimToken<std::string>(TokenType::STRING, "session-key"),
        PrimToken<int>(TokenType::INT, 30)
    };

    auto parsed = Parser::try_parse_expire<std::string>(tokens);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_NE(parsed.value(), nullptr);
    EXPECT_EQ(parsed.value()->get_type(), StatementType::EXPIRE);
    EXPECT_EQ(parsed.value()->key(), "session-key");
    EXPECT_EQ(parsed.value()->expire_time(), 30);
}

TEST(ParserTest, TryParseExpireRejectsNonIntegerTtl) {
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        PrimToken<std::string>(TokenType::STRING, "session-key"),
        PrimToken<double>(TokenType::DOUBLE, 30.5)
    };

    auto parsed = Parser::try_parse_expire<std::string>(tokens);

    EXPECT_FALSE(parsed.has_value());
}

TEST(ParserTest, ParseDispatchesSetStatements) {
    std::vector<Token> tokens = {
        Token(TokenType::SET),
        PrimToken<std::string>(TokenType::STRING, "user"),
        PrimToken<std::string>(TokenType::STRING, "mark")
    };

    auto parsed = Parser::parse(tokens);

    ASSERT_TRUE(parsed.has_value());
    ASSERT_NE(parsed.value(), nullptr);
    EXPECT_EQ(parsed.value()->get_type(), StatementType::SET);

    auto* set_statement = dynamic_cast<SetStatement<std::string, std::string>*>(parsed.value().get());
    ASSERT_NE(set_statement, nullptr);
    EXPECT_EQ(set_statement->key(), "user");
    EXPECT_EQ(set_statement->value(), "mark");
}

TEST(ParserTest, ParseRejectsMalformedCommands) {
    std::vector<Token> tokens = {
        Token(TokenType::EXPIRE),
        PrimToken<std::string>(TokenType::STRING, "session-key")
    };

    auto parsed = Parser::parse(tokens);

    EXPECT_FALSE(parsed.has_value());
}
