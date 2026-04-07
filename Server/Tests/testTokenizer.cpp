//
// Created by Mark on 4/5/26.
//

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

#include "../Interpreter/PrimToken.h"
#include "../Interpreter/Token.h"
#include "../Interpreter/TokenType.h"
#include "../Interpreter/Tokenizer.h"

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
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntWithLeadingWhitespace) {
    std::string input = "   123";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntWithTrailingWhitespace) {
    std::string input = "123   ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntWithLeadingAndTrailingWhitespace) {
    std::string input = "   123   ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, ZeroInt) {
    std::string input = "0";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 0))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, MultipleZerosInt) {
    std::string input = "0000";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 0))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, LargeInt) {
    std::string input = "2147483647";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 2147483647))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, MultipleInts) {
    std::string input = "123 456 789";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 456)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 789))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntThenCommand) {
    std::string input = "123 GET";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123)),
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
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, CommandIntCommandInt) {
    std::string input = "SET 10 GET 20";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 10)),
        Token(TokenType::GET),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 20))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, IntBetweenCommandsWithExtraWhitespace) {
    std::string input = "   SET    42    DEL   99   ";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        Token(TokenType::SET),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 42)),
        Token(TokenType::DEL),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 99))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, NegativeInt) {
    std::string input = "-123";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, -123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, NegativeZeroInt) {
    std::string input = "-0";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 0))
    };

    EXPECT_EQ(tokens, expected);
}


TEST(TokenizerTest, MinInt) {
    std::string input = "-2147483648";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, -2147483648))
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

    EXPECT_TRUE(result.has_value());
}

TEST(TokenizerTest, CommandAdjacentToIntFails) {
    std::string input = "SET123";
    auto result = Tokenizer::tokenize(input);

    EXPECT_TRUE(result.has_value());
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
        static_cast<Token>(PrimToken<int>(TokenType::INT, 123)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, -456))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, ManyInts) {
    std::string input = "1 22 333 4444 55555";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<int>(TokenType::INT, 1)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 22)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 333)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 4444)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 55555))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, DoubleThenNegativeDouble) {
    std::string input = "123.123 -456.456";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    for (auto & token : tokens) {
        std::cout << token_type_str(token.get_type()) << " " << std::endl;
    }
    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<double>(TokenType::DOUBLE, 123.123)),
        static_cast<Token>(PrimToken<double>(TokenType::DOUBLE, -456.123))
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, TestBasicChar) {
    std::string input = "'p'";

    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);


    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<double>(TokenType::CHAR, 'p')),
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, TestBasicString) {
    std::string input = "\"67\"";

    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    for (auto token : tokens) {
        std::cout << token_type_str(token.get_type()) << std::endl;
    }

    std::vector<Token> expected = {
        static_cast<Token>(PrimToken<std::string>(TokenType::STRING, "67")),
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, TestBasicSetFunction) {
    std::string input = "SET 67 54";

    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    for (auto token : tokens) {
        std::cout << token_type_str(token.get_type()) << std::endl;
    }

    std::vector<Token> expected = {
        Token(TokenType::SET),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 67)),
        static_cast<Token>(PrimToken<int>(TokenType::INT, 54)),
    };

    EXPECT_EQ(tokens, expected);
}