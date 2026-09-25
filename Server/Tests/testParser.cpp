#include <gtest/gtest.h>

#include "Command.h"
#include "Parser.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
Command expect_command(const CommandArguments& arguments) {
    ParseResult parsed = Parser::parse_request(arguments);
    if (const auto* error = std::get_if<ParseError>(&parsed)) {
        ADD_FAILURE() << "unexpected parse error: " << error->message;
        return GetCommand{};
    }
    return std::get<Command>(std::move(parsed));
}

std::string expect_error(const CommandArguments& arguments) {
    const ParseResult parsed = Parser::parse_request(arguments);
    const auto* error = std::get_if<ParseError>(&parsed);
    if (!error) {
        ADD_FAILURE() << "expected a parse error";
        return {};
    }
    return error->message;
}
} // namespace

TEST(ParserTest, ParsesGetWithByteKey) {
    const Command command = expect_command({"GET", "session-key"});
    EXPECT_EQ(std::get<GetCommand>(command).key, "session-key");
}

TEST(ParserTest, CommandNameIsCaseInsensitive) {
    EXPECT_TRUE(std::holds_alternative<GetCommand>(expect_command({"get", "k"})));
    EXPECT_TRUE(std::holds_alternative<GetCommand>(expect_command({"GeT", "k"})));
}

TEST(ParserTest, KeysAndValuesAreNotNumericallyNormalized) {
    for (const Bytes key : {"007", "1.50", "1e3", "-0", "+5"}) {
        SCOPED_TRACE(key);
        const auto set = std::get<SetCommand>(expect_command({"SET", key, key}));
        EXPECT_EQ(set.key, key);
        EXPECT_EQ(set.value, key);
    }
}

TEST(ParserTest, CommandNamesAreBytesWhenUsedAsArguments) {
    const auto set = std::get<SetCommand>(expect_command({"SET", "get", "set"}));
    EXPECT_EQ(set.key, "get");
    EXPECT_EQ(set.value, "set");
}

TEST(ParserTest, ArgumentsPreserveBinaryBytes) {
    const Bytes key{"k\0\xff", 3};
    const Bytes value{"\r\n\"", 3};
    const auto set = std::get<SetCommand>(expect_command({"SET", key, value}));
    EXPECT_EQ(set.key, key);
    EXPECT_EQ(set.value, value);
}

TEST(ParserTest, ParsesDeleteAndExistsCommands) {
    EXPECT_EQ(std::get<DeleteCommand>(expect_command({"DEL", "7"})).key, "7");
    EXPECT_EQ(std::get<ExistsCommand>(expect_command({"EXISTS", "k"})).key, "k");
}

TEST(ParserTest, ParsesExpireAsAbsoluteDeadline) {
    const auto before = ExpireCommand::Clock::now();
    const auto command = std::get<ExpireCommand>(expect_command({"EXPIRE", "session-key", "30"}));
    const auto after = ExpireCommand::Clock::now();
    EXPECT_EQ(command.key, "session-key");
    EXPECT_GE(command.expires_at, before + std::chrono::seconds(30));
    EXPECT_LE(command.expires_at, after + std::chrono::seconds(30));
}

TEST(ParserTest, ExpireAcceptsNegativeSeconds) {
    const auto before = ExpireCommand::Clock::now();
    const auto command = std::get<ExpireCommand>(expect_command({"EXPIRE", "k", "-1"}));
    EXPECT_LE(command.expires_at, before + std::chrono::seconds(1));
}

TEST(ParserTest, ExpireRejectsNonCanonicalIntegers) {
    for (const Bytes seconds : {"+30", "030", "-0", "30.5", "3e1", "", " 30", "thirty",
                                "9223372036854775808"}) {
        SCOPED_TRACE(seconds);
        EXPECT_EQ(expect_error({"EXPIRE", "k", seconds}), "value is not an integer or out of range");
    }
}

TEST(ParserTest, RejectsExpireOutsideClockRange) {
    for (const Bytes seconds : {"9223372036854775807", "-9223372036854775808"}) {
        SCOPED_TRACE(seconds);
        EXPECT_EQ(expect_error({"EXPIRE", "k", seconds}), "invalid expire time in 'expire' command");
    }
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

TEST(ParserTest, ClientRequestsCannotUseInternalAofCommands) {
    EXPECT_EQ(expect_error({"PEXPIREAT", "k", "4102444800000"}), "unknown command");
    EXPECT_EQ(expect_error({"SETSTATE", "k", "v", "PERSIST"}), "unknown command");
}

TEST(ParserTest, TtlPttlAndPersistHaveDistinctAlternatives) {
    EXPECT_TRUE(std::holds_alternative<TtlCommand>(expect_command({"TTL", "key"})));
    EXPECT_TRUE(std::holds_alternative<PttlCommand>(expect_command({"PTTL", "key"})));
    EXPECT_TRUE(std::holds_alternative<PersistCommand>(expect_command({"PERSIST", "key"})));
}

TEST(ParserTest, ParseArgumentsSupportsPersistForAofReplay) {
    auto parsed = Parser::parse_arguments({"PERSIST", "session-key"});
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(std::holds_alternative<PersistCommand>(*parsed));
    EXPECT_TRUE(is_mutating(*parsed));
}

TEST(ParserTest, ReportsEmptyAndUnknownCommands) {
    EXPECT_EQ(expect_error({}), "empty command");
    EXPECT_EQ(expect_error({"UNKNOWN", "k"}), "unknown command");
    EXPECT_EQ(expect_error({Bytes{"GET\0", 4}, "k"}), "unknown command");
}

TEST(ParserTest, ReportsWrongArgumentCountWithCommandName) {
    for (const auto& [arguments, name] : std::vector<std::pair<CommandArguments, std::string>>{
             {{"get"}, "get"},
             {{"SET", "k"}, "set"},
             {{"DEL", "k", "extra"}, "del"},
             {{"EXPIRE", "k"}, "expire"},
             {{"Incr"}, "incr"},
             {{"INCR", "counter", "extra"}, "incr"},
             {{"DECR"}, "decr"},
             {{"INCRBY", "counter"}, "incrby"},
             {{"DECRBY", "counter", "1", "extra"}, "decrby"}}) {
        SCOPED_TRACE(name);
        EXPECT_EQ(expect_error(arguments), "wrong number of arguments for '" + name + "' command");
    }
}

TEST(ParserTest, NumericCommandsHaveDistinctCommandAlternatives) {
    std::unordered_set<std::size_t> alternatives;
    for (const CommandArguments& arguments : std::vector<CommandArguments>{
             {"INCR", "counter"}, {"DECR", "counter"},
             {"INCRBY", "counter", "5"}, {"DECRBY", "counter", "5"}}) {
        alternatives.insert(expect_command(arguments).index());
    }

    EXPECT_EQ(alternatives.size(), 4U);
}

TEST(ParserTest, ByCommandsPreserveOperandBytesUntilExecution) {
    for (const Bytes amount : {"001", "-0", "5", "+5", "1.5"}) {
        SCOPED_TRACE(amount);
        const CommandArguments arguments{"INCRBY", "counter", amount};
        EXPECT_EQ(command_arguments(expect_command(arguments)), arguments);
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
