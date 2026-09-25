#include "Commands/Parser.h"

#include "Core/Integer.h"

#include <charconv>
#include <chrono>
#include <cctype>
#include <string>
#include <string_view>

namespace {
constexpr std::size_t kCommandIndex = 0;
constexpr std::size_t kKeyIndex = 1;
constexpr std::size_t kSecondArgumentIndex = 2;

struct KeyCommandSpec {
    std::string_view name;
    std::size_t argument_count;
    Command (*build)(const CommandArguments& arguments);
};

// Commands whose arguments are all byte strings, shared by client requests and AOF replay.
const KeyCommandSpec kKeyCommands[] = {
    {"GET", 2, [](const CommandArguments& a) -> Command { return GetCommand{a[1]}; }},
    {"SET", 3, [](const CommandArguments& a) -> Command { return SetCommand{a[1], a[2]}; }},
    {"DEL", 2, [](const CommandArguments& a) -> Command { return DeleteCommand{a[1]}; }},
    {"EXISTS", 2, [](const CommandArguments& a) -> Command { return ExistsCommand{a[1]}; }},
    {"TTL", 2, [](const CommandArguments& a) -> Command { return TtlCommand{a[1]}; }},
    {"PTTL", 2, [](const CommandArguments& a) -> Command { return PttlCommand{a[1]}; }},
    {"PERSIST", 2, [](const CommandArguments& a) -> Command { return PersistCommand{a[1]}; }},
    {"INCR", 2, [](const CommandArguments& a) -> Command { return IncrCommand{a[1]}; }},
    {"DECR", 2, [](const CommandArguments& a) -> Command { return DecrCommand{a[1]}; }},
    // The amount stays as bytes; StorageEngine validates it at execution time.
    {"INCRBY", 3, [](const CommandArguments& a) -> Command { return IncrByCommand{a[1], a[2]}; }},
    {"DECRBY", 3, [](const CommandArguments& a) -> Command { return DecrByCommand{a[1], a[2]}; }},
};

constexpr std::string_view kExpireName = "EXPIRE";
constexpr std::size_t kExpireArgumentCount = 3;

std::string uppercase_ascii(const Bytes& bytes) {
    std::string result;
    result.reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        if (byte > 0x7f) return {};
        result.push_back(static_cast<char>(std::toupper(byte)));
    }
    return result;
}

std::string lowercase_ascii(const std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const unsigned char byte : text) result.push_back(static_cast<char>(std::tolower(byte)));
    return result;
}

const KeyCommandSpec* find_key_command(const std::string_view name) {
    for (const KeyCommandSpec& spec : kKeyCommands) {
        if (spec.name == name) return &spec;
    }
    return nullptr;
}

ParseError wrong_argument_count(const std::string_view name) {
    return {"wrong number of arguments for '" + lowercase_ascii(name) + "' command"};
}

ParseResult parse_expire(const CommandArguments& arguments) {
    if (arguments.size() != kExpireArgumentCount) return wrong_argument_count(kExpireName);
    const auto seconds = parse_integer(arguments[kSecondArgumentIndex]);
    if (!seconds) return ParseError{"value is not an integer or out of range"};

    const auto now = ExpireCommand::Clock::now();
    const auto requested = std::chrono::duration<long double>(*seconds);
    const auto current_offset = std::chrono::duration<long double>(now.time_since_epoch());
    const auto maximum_offset =
        std::chrono::duration<long double>(ExpireCommand::TimePoint::max().time_since_epoch());
    const auto minimum_offset =
        std::chrono::duration<long double>(ExpireCommand::TimePoint::min().time_since_epoch());
    if (requested > maximum_offset - current_offset ||
        requested < minimum_offset - current_offset) {
        return ParseError{"invalid expire time in 'expire' command"};
    }
    return ExpireCommand{arguments[kKeyIndex], now + std::chrono::seconds(*seconds)};
}

std::optional<ExpireCommand::TimePoint> parse_millisecond_deadline(const Bytes& bytes) {
    std::int64_t unix_milliseconds = 0;
    const char* first = bytes.data();
    const char* last = first + bytes.size();
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
    return ExpireCommand::TimePoint(duration);
}
}

ParseResult Parser::parse_request(const CommandArguments& arguments) {
    if (arguments.empty()) return ParseError{"empty command"};
    const std::string command = uppercase_ascii(arguments[kCommandIndex]);
    if (command == kExpireName) return parse_expire(arguments);

    const KeyCommandSpec* spec = find_key_command(command);
    if (!spec) return ParseError{"unknown command"};
    if (arguments.size() != spec->argument_count) return wrong_argument_count(spec->name);
    return spec->build(arguments);
}

std::optional<Command> Parser::parse_arguments(const CommandArguments& arguments) {
    if (arguments.empty()) return std::nullopt;
    const std::string command = uppercase_ascii(arguments[kCommandIndex]);
    if (const KeyCommandSpec* spec = find_key_command(command)) {
        if (arguments.size() != spec->argument_count) return std::nullopt;
        return spec->build(arguments);
    }
    if (command == "PEXPIREAT" && arguments.size() == 3) {
        auto deadline = parse_millisecond_deadline(arguments[2]);
        if (!deadline) return std::nullopt;
        return ExpireCommand{arguments[1], *deadline};
    }
    if (command == "SETSTATE" && arguments.size() == 4) {
        if (arguments[3] == "PERSIST") return SetStateCommand{arguments[1], arguments[2], std::nullopt};
        auto deadline = parse_millisecond_deadline(arguments[3]);
        if (!deadline) return std::nullopt;
        return SetStateCommand{arguments[1], arguments[2], *deadline};
    }
    return std::nullopt;
}
