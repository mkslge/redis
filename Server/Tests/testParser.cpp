#include <gtest/gtest.h>

#include "Command.h"
#include "Parser.h"
#include "Token.h"
#include "TokenType.h"
#include "Tokenizer.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

TEST(ParserTest, ParsesGetWithPrimitiveKey) {
    std::vector<Token> tokens{Token(TokenType::GET), Token(TokenType::STRING, "session-key")};
    auto parsed = Parser::parse(tokens);
    ASSERT_TRUE(parsed.has_value());
    const auto* command = std::get_if<GetCommand>(&*parsed);
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->key, "session-key");
}

TEST(ParserTest, RejectsGetWithNonPrimitiveKey) {
    std::vector<Token> tokens{Token(TokenType::GET), Token(TokenType::SET)};
    EXPECT_FALSE(Parser::parse(tokens).has_value());
}

TEST(ParserTest, ParsesSetWithByteValue) {
    std::vector<Token> tokens{
        Token(TokenType::SET), Token(TokenType::STRING, "name"), Token(TokenType::INT, 42)};
    auto parsed = Parser::parse(tokens);
    ASSERT_TRUE(parsed.has_value());
    const auto* command = std::get_if<SetCommand>(&*parsed);
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(command->key, "name");
    EXPECT_EQ(command->value, "42");
}

TEST(ParserTest, ParsesDeleteAndExistsCommands) {
    std::vector<Token> delete_tokens{Token(TokenType::DEL), Token(TokenType::INT, 7)};
    std::vector<Token> exists_tokens{Token(TokenType::EXISTS), Token(TokenType::CHAR, 'k')};
    auto deletion = Parser::parse(delete_tokens);
    auto exists = Parser::parse(exists_tokens);
    ASSERT_TRUE(deletion.has_value());
    ASSERT_TRUE(exists.has_value());
    EXPECT_EQ(std::get<DeleteCommand>(*deletion).key, "7");
    EXPECT_EQ(std::get<ExistsCommand>(*exists).key, "k");
}

TEST(ParserTest, ParsesExpireAsAbsoluteDeadline) {
    const auto before = ExpireCommand::Clock::now();
    std::vector<Token> tokens{
        Token(TokenType::EXPIRE), Token(TokenType::STRING, "session-key"), Token(TokenType::INT, 30)};
    auto parsed = Parser::parse(tokens);
    const auto after = ExpireCommand::Clock::now();
    ASSERT_TRUE(parsed.has_value());
    const auto& command = std::get<ExpireCommand>(*parsed);
    EXPECT_EQ(command.key, "session-key");
    EXPECT_GE(command.expires_at, before + std::chrono::seconds(30));
    EXPECT_LE(command.expires_at, after + std::chrono::seconds(30));
}

TEST(ParserTest, ParseArgumentsReadsAbsoluteExpirationFromAof) {
    constexpr std::int64_t deadline_milliseconds = 4'102'444'800'000;
    auto parsed = Parser::parse_arguments(
        {"PEXPIREAT", "session-key", std::to_string(deadline_milliseconds)});
    ASSERT_TRUE(parsed.has_value());
    const auto& command = std::get<ExpireCommand>(*parsed);
    EXPECT_EQ(command.key, "session-key");
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::milliseconds>(
                  command.expires_at.time_since_epoch()).count(), deadline_milliseconds);
}

TEST(ParserTest, ParseArgumentsRejectsRelativeExpireInAof) {
    EXPECT_FALSE(Parser::parse_arguments({"EXPIRE", "session-key", "30"}).has_value());
}

TEST(ParserTest, RejectsNonIntegerExpire) {
    std::vector<Token> tokens{
        Token(TokenType::EXPIRE), Token(TokenType::STRING, "session-key"), Token(TokenType::DOUBLE, 30.5)};
    EXPECT_FALSE(Parser::parse(tokens).has_value());
}

TEST(ParserTest, RejectsExpireOutsideClockRange) {
    for (const std::string command_line : {
             "EXPIRE \"key\" 9223372036854775807",
             "EXPIRE \"key\" -9223372036854775808"}) {
        const auto tokens = Tokenizer::tokenize(command_line);
        ASSERT_TRUE(tokens.has_value());
        EXPECT_FALSE(Parser::parse(*tokens).has_value());
    }
}

TEST(ParserTest, TtlPttlAndPersistHaveDistinctAlternatives) {
    auto ttl_tokens = Tokenizer::tokenize("TTL \"key\"");
    auto pttl_tokens = Tokenizer::tokenize("PTTL \"key\"");
    auto persist_tokens = Tokenizer::tokenize("PERSIST \"key\"");
    ASSERT_TRUE(ttl_tokens && pttl_tokens && persist_tokens);
    EXPECT_TRUE(std::holds_alternative<TtlCommand>(*Parser::parse(*ttl_tokens)));
    EXPECT_TRUE(std::holds_alternative<PttlCommand>(*Parser::parse(*pttl_tokens)));
    EXPECT_TRUE(std::holds_alternative<PersistCommand>(*Parser::parse(*persist_tokens)));
}

TEST(ParserTest, ParseArgumentsSupportsPersistForAofReplay) {
    auto parsed = Parser::parse_arguments({"PERSIST", "session-key"});
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(std::holds_alternative<PersistCommand>(*parsed));
    EXPECT_TRUE(is_mutating(*parsed));
}

TEST(ParserTest, RejectsMalformedCommand) {
    std::vector<Token> tokens{Token(TokenType::EXPIRE), Token(TokenType::STRING, "session-key")};
    EXPECT_FALSE(Parser::parse(tokens).has_value());
}

TEST(ParserTest, NumericCommandsHaveDistinctCommandAlternatives) {
    std::unordered_set<std::size_t> alternatives;
    for (const std::string command_line : {
             "INCR \"counter\"", "DECR \"counter\"",
             "INCRBY \"counter\" 5", "DECRBY \"counter\" 5"}) {
        const auto tokens = Tokenizer::tokenize(command_line);
        ASSERT_TRUE(tokens.has_value()) << command_line;
        const auto command = Parser::parse(*tokens);
        ASSERT_TRUE(command.has_value()) << command_line;
        alternatives.insert(command->index());
    }

    EXPECT_EQ(alternatives.size(), 4U);
}

TEST(ParserTest, NumericCommandsRequireExactArity) {
    for (const std::string command_line : {
             "INCR", "INCR \"counter\" \"extra\"",
             "DECR", "DECR \"counter\" \"extra\"",
             "INCRBY \"counter\"", "INCRBY \"counter\" 1 \"extra\"",
             "DECRBY \"counter\"", "DECRBY \"counter\" 1 \"extra\""}) {
        const auto tokens = Tokenizer::tokenize(command_line);
        ASSERT_TRUE(tokens.has_value()) << command_line;
        EXPECT_FALSE(Parser::parse(*tokens).has_value()) << command_line;
    }
}

TEST(ParserTest, ByCommandsPreserveOperandBytesUntilExecution) {
    for (const auto& [command_line, expected_amount] :
         std::vector<std::pair<std::string, Bytes>>{
             {"INCRBY \"counter\" 001", "001"},
             {"DECRBY \"counter\" \"-0\"", "-0"},
             {"INCRBY \"counter\" \"5\"", "5"}}) {
        const auto tokens = Tokenizer::tokenize(command_line);
        ASSERT_TRUE(tokens.has_value()) << command_line;
        const auto command = Parser::parse(*tokens);
        ASSERT_TRUE(command.has_value()) << command_line;
        const CommandArguments arguments = command_arguments(*command);
        ASSERT_EQ(arguments.size(), 3U);
        EXPECT_EQ(arguments[1], "counter");
        EXPECT_EQ(arguments[2], expected_amount);
    }
}

TEST(ParserTest, ParseArgumentsSupportsNumericCommandsAndBinaryOperands) {
    const Bytes binary_amount{"1\0", 2};
    for (const CommandArguments arguments : {
             CommandArguments{"INCR", "counter"},
             CommandArguments{"DECR", "counter"},
             CommandArguments{"INCRBY", "counter", binary_amount},
             CommandArguments{"DECRBY", "counter", "-5"}}) {
        const auto command = Parser::parse_arguments(arguments);
        ASSERT_TRUE(command.has_value());
        EXPECT_TRUE(is_mutating(*command));
        EXPECT_EQ(command_arguments(*command), arguments);
    }
}
