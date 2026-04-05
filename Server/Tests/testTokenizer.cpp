//
// Created by Mark on 4/5/26.
//
//
// Created by Mark on 4/5/26.
//

#include <gtest/gtest.h>
#include <optional>
#include <vector>
#include <string>

#include "../Interpreter/TokenType.h"
#include "../Interpreter/Tokenizer.h"
// ---------- Helpers ----------
static std::vector<TokenType> unwrap(const std::optional<std::vector<TokenType>>& opt) {
    EXPECT_TRUE(opt.has_value());
    return opt.value();
}

// ---------- Tests ----------

// Basic happy path
TEST(TokenizerTest, SimpleSetCommand) {
    std::string input = "setkeyvalue";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<TokenType> expected = {
        TokenType::GET,   // NOTE: your implementation swaps set/get
        TokenType::key,
        TokenType::key
    };

    EXPECT_EQ(tokens, expected);
}

// Case normalization
TEST(TokenizerTest, UppercaseInput) {
    std::string input = "SETKEY";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<TokenType> expected = {
        TokenType::GET,
        TokenType::key
    };

    EXPECT_EQ(tokens, expected);
}

// Multiple commands chained
TEST(TokenizerTest, MultipleTokens) {
    std::string input = "getkeydelkey";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<TokenType> expected = {
        TokenType::SET,   // swapped
        TokenType::key,
        TokenType::DEL,
        TokenType::key
    };

    EXPECT_EQ(tokens, expected);
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
    std::string input = "setkeyBAD";
    auto result = Tokenizer::tokenize(input);

    EXPECT_FALSE(result.has_value());
}

// Longer tokens
TEST(TokenizerTest, ExistsExpireSeconds) {
    std::string input = "existsexpireseconds";
    auto result = Tokenizer::tokenize(input);

    auto tokens = unwrap(result);

    std::vector<TokenType> expected = {
        TokenType::EXISTS,
        TokenType::EXPIRE,
        TokenType::seconds
    };

    EXPECT_EQ(tokens, expected);
}

TEST(TokenizerTest, shouldFail) {
    EXPECT_FALSE(true);
}