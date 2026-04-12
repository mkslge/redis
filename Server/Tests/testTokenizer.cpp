//
// Created by Mark on 4/5/26.
//

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

#include "Parsing/Token.h"
#include "Parsing/TokenType.h"
#include "Parsing/Tokenizer.h"

// ---------- Helpers ----------
static std::vector<Token> unwrap(const std::optional<std::vector<Token>>& opt) {
    EXPECT_TRUE(opt.has_value());
    return opt.value();
}

// ---------- Tests ----------

// Basic happy path
TEST(TokenizerTest, SimpleSetCommand) {
    std::string input = "setkeyvalueBLA";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

// Case normalization
TEST(TokenizerTest, UppercaseInput) {
    std::string input = "SET";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
    };

    EXPECT_EQ(tokens, expected);
}

// Multiple commands chained
TEST(TokenizerTest, MultipleTokens) {
    std::string input = "get BAD del BAD";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

// Invalid input should fail
TEST(TokenizerTest, InvalidToken) {
    std::string input = "unknown";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

// Partial match should fail
TEST(TokenizerTest, PartialToken) {
    std::string input = "se";  // incomplete
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

// Mixed valid + invalid
TEST(TokenizerTest, StopsOnInvalid) {
    std::string input = "SET KEY BAD";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

// Longer tokens
TEST(TokenizerTest, ExistsExpireSeconds) {
    std::string input = "   EXISTS EXPIRE ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::EXISTS),
        Token(TokenType::EXPIRE)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, BasicInt) {
    std::string input = "123";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntWithLeadingWhitespace) {
    std::string input = "   123";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntWithTrailingWhitespace) {
    std::string input = "123   ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntWithLeadingAndTrailingWhitespace) {
    std::string input = "   123   ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, ZeroInt) {
    std::string input = "0";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 0)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, MultipleZerosInt) {
    std::string input = "0000";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 0)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, LargeInt) {
    std::string input = "2147483647";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 2147483647)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, MultipleInts) {
    std::string input = "123 456 789";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123),
        Token(TokenType::INT, 456),
        Token(TokenType::INT, 789)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntThenCommand) {
    std::string input = "123 GET";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123),
        Token(TokenType::GET)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, CommandThenInt) {
    std::string input = "SET 123";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
        Token(TokenType::INT, 123)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, CommandIntCommandInt) {
    std::string input = "SET 10 GET 20";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
        Token(TokenType::INT, 10),
        Token(TokenType::GET),
        Token(TokenType::INT, 20)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntBetweenCommandsWithExtraWhitespace) {
    std::string input = "   SET    42    DEL   99   ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
        Token(TokenType::INT, 42),
        Token(TokenType::DEL),
        Token(TokenType::INT, 99)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, NegativeInt) {
    std::string input = "-123";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, -123)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, NegativeZeroInt) {
    std::string input = "-0";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 0)
    };

    EXPECT_EQ(tokens, expected);
}


TEST(TokenizerTest, MinInt) {
    std::string input = "-2147483648";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, static_cast<int>(-2147483648LL))
    };

    EXPECT_EQ(tokens, expected);
}



TEST(TokenizerTest, IntFollowedByLettersFails) {
    std::string input = "123abc";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, LettersBeforeIntFails) {
    std::string input = "abc123";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, IntWithEmbeddedLetterFails) {
    std::string input = "12a3";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}



TEST(TokenizerTest, NegativeIntWithLettersFails) {
    std::string input = "-123abc";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, PlusOnlyFails) {
    std::string input = "+";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, MinusOnlyFails) {
    std::string input = "-";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, EmptyStringFails) {
    std::string input = "";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, WhitespaceOnlyFails) {
    std::string input = "      ";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, TwoIntsWithoutSeparatorFails) {
    std::string input = "123456abc789";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, IntAdjacentToCommandFails) {
    std::string input = "123GET";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, CommandAdjacentToIntFails) {
    std::string input = "SET123";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, DelAdjacentToIntFails) {
    std::string input = "DEL99";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, ExistsAdjacentToStringFails) {
    std::string input = "EXISTS\"key\"";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, ExpireAdjacentToKeyFails) {
    std::string input = "EXPIRE\"session\" 30";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, AdjacentCommandsWithoutWhitespaceFail) {
    std::string input = "GETDEL";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, SequenceOfValidAndInvalidIntTokensFails) {
    std::string input = "123 456x 789";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

TEST(TokenizerTest, IntThenNegativeInt) {
    std::string input = "123 -456";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 123),
        Token(TokenType::INT, -456)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, ManyInts) {
    std::string input = "1 22 333 4444 55555";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::INT, 1),
        Token(TokenType::INT, 22),
        Token(TokenType::INT, 333),
        Token(TokenType::INT, 4444),
        Token(TokenType::INT, 55555)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, DoubleThenNegativeDouble) {
    std::string input = "123.123 -456.456";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::DOUBLE, 123.123),
        Token(TokenType::DOUBLE, -456.456)
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, TestBasicChar) {
    std::string input = "'p'";

    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);


    std::vector<Token> expected = {
        Token(TokenType::CHAR, 'p'),
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, TestBasicString) {
    std::string input = "\"67\"";

    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::STRING, "67"),
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, TestBasicSetFunction) {
    std::string input = "SET 67 54";

    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
        Token(TokenType::INT, 67),
        Token(TokenType::INT, 54),
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, PreservesCaseForStringAndCharValues) {
    std::string string_input = "SET \"Hello\" 'X'";
    auto string_result = Tokenizer::tokenize(string_input);

    auto string_tokens = unwrap(string_result);

    std::vector<Token> expected_string_tokens = {
        Token(TokenType::SET),
        Token(TokenType::STRING, "Hello"),
        Token(TokenType::CHAR, 'X'),
    };

    EXPECT_EQ(string_tokens, expected_string_tokens);
}
