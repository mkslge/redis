#include "Parser.h"

#include <charconv>
#include <chrono>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>

namespace {
std::string uppercase_ascii(const Bytes& bytes) {
    std::string result;
    result.reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        if (byte > 0x7f) return {};
        result.push_back(static_cast<char>(std::toupper(byte)));
    }
    return result;
}
}

std::optional<Command> Parser::parse_arguments(const CommandArguments& arguments) {
    if (arguments.empty()) return std::nullopt;
    const std::string command = uppercase_ascii(arguments[0]);
    if (command == "GET" && arguments.size() == 2) return GetCommand{arguments[1]};
    if (command == "SET" && arguments.size() == 3) return SetCommand{arguments[1], arguments[2]};
    if (command == "DEL" && arguments.size() == 2) return DeleteCommand{arguments[1]};
    if (command == "EXISTS" && arguments.size() == 2) return ExistsCommand{arguments[1]};
    if (command == "TTL" && arguments.size() == 2) return TtlCommand{arguments[1]};
    if (command == "PTTL" && arguments.size() == 2) return PttlCommand{arguments[1]};
    if (command == "PERSIST" && arguments.size() == 2) return PersistCommand{arguments[1]};
    if (command != "PEXPIREAT" || arguments.size() != 3) return std::nullopt;

    std::int64_t unix_milliseconds = 0;
    const char* first = arguments[2].data();
    const char* last = first + arguments[2].size();
    const auto parsed = std::from_chars(first, last, unix_milliseconds);
    if (parsed.ec != std::errc{} || parsed.ptr != last) return std::nullopt;

    using Milliseconds = std::chrono::milliseconds;
    const auto minimum = std::chrono::duration_cast<Milliseconds>(
        ExpireCommand::TimePoint::min().time_since_epoch()).count();
    const auto maximum = std::chrono::duration_cast<Milliseconds>(
        ExpireCommand::TimePoint::max().time_since_epoch()).count();
    if (unix_milliseconds < minimum || unix_milliseconds > maximum) return std::nullopt;

    const auto duration = std::chrono::duration_cast<ExpireCommand::Clock::duration>(
        Milliseconds(unix_milliseconds));
    return ExpireCommand{arguments[1], ExpireCommand::TimePoint(duration)};
}

std::optional<Command> Parser::parse(const std::vector<Token>& tokens) {
    if (tokens.empty()) return std::nullopt;
    const TokenType type = tokens[kCommandTokenIndex].get_type();

    if (type == TokenType::SET) {
        if (tokens.size() != kBinaryCommandTokenCount ||
            !tokens[kFirstArgumentTokenIndex].has_value() ||
            !tokens[kSecondArgumentTokenIndex].has_value()) return std::nullopt;
        auto key = key_from_token(tokens[kFirstArgumentTokenIndex]);
        auto value = value_from_token(tokens[kSecondArgumentTokenIndex]);
        if (!key || !value) return std::nullopt;
        return SetCommand{std::move(*key), std::move(*value)};
    }

    if (type == TokenType::EXPIRE) {
        if (tokens.size() != kBinaryCommandTokenCount ||
            tokens[kSecondArgumentTokenIndex].get_type() != TokenType::INT ||
            !tokens[kFirstArgumentTokenIndex].has_value()) return std::nullopt;
        auto key = key_from_token(tokens[kFirstArgumentTokenIndex]);
        auto seconds = tokens[kSecondArgumentTokenIndex].template get_prim<int>();
        if (!key || !seconds) return std::nullopt;
        return ExpireCommand{std::move(*key),
            ExpireCommand::Clock::now() + std::chrono::seconds(*seconds)};
    }

    if (tokens.size() != kUnaryCommandTokenCount ||
        !tokens[kFirstArgumentTokenIndex].has_value()) return std::nullopt;
    auto key = key_from_token(tokens[kFirstArgumentTokenIndex]);
    if (!key) return std::nullopt;

    switch (type) {
        case TokenType::GET: return GetCommand{std::move(*key)};
        case TokenType::DEL: return DeleteCommand{std::move(*key)};
        case TokenType::EXISTS: return ExistsCommand{std::move(*key)};
        case TokenType::TTL: return TtlCommand{std::move(*key)};
        case TokenType::PTTL: return PttlCommand{std::move(*key)};
        case TokenType::PERSIST: return PersistCommand{std::move(*key)};
        default: return std::nullopt;
    }
}

std::optional<Key> Parser::key_from_token(const Token& token) {
    switch (token.get_type()) {
        case TokenType::INT:
            return std::to_string(token.template get_prim<int>().value());
        case TokenType::DOUBLE: {
            std::ostringstream stream;
            stream.precision(std::numeric_limits<double>::max_digits10);
            stream << token.template get_prim<double>().value();
            return stream.str();
        }
        case TokenType::CHAR:
            return std::string(1, token.template get_prim<char>().value());
        case TokenType::STRING:
            return token.template get_prim<std::string>().value();
        default:
            return std::nullopt;
    }
}

std::optional<Bytes> Parser::value_from_token(const Token& token) {
    if (token.source_text().has_value()) return token.source_text().value();
    return key_from_token(token);
}
