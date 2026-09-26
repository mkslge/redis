#include "Commands/Parser.h"

#include "Commands/Errors.h"
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

template<class T>
KeyCommandSpec key_only() {
    return {T::name, 2, [](const CommandArguments& a) -> Command { return T{a[1]}; }};
}

// The amount stays as bytes; StorageEngine validates it at execution time.
template<class T>
KeyCommandSpec key_and_amount() {
    return {T::name, 3, [](const CommandArguments& a) -> Command { return T{a[1], a[2]}; }};
}

// Commands whose arguments are all byte strings, shared by client requests and AOF replay.
const KeyCommandSpec kKeyCommands[] = {
    key_only<GetCommand>(),
    {SetCommand::name, 3, [](const CommandArguments& a) -> Command { return SetCommand{a[1], a[2]}; }},
    key_only<DeleteCommand>(),
    key_only<ExistsCommand>(),
    key_only<TtlCommand>(),
    key_only<PttlCommand>(),
    key_only<PersistCommand>(),
    key_only<IncrCommand>(),
    key_only<DecrCommand>(),
    key_and_amount<IncrByCommand>(),
    key_and_amount<DecrByCommand>(),
};

constexpr std::size_t kExpireArgumentCount = 3;

std::string uppercase_ascii(const Bytes& bytes) {
    std::string result;
    result.reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        // Bytes above 0x7f are not ASCII, so no command name contains them. Returning
        // an empty name makes the lookup fail with "unknown command".
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
    return {Errors::wrong_argument_count(lowercase_ascii(name))};
}

ParseResult parse_expire(const CommandArguments& arguments) {
    if (arguments.size() != kExpireArgumentCount) return wrong_argument_count(ExpireCommand::name);
    const auto seconds = parse_integer(arguments[kSecondArgumentIndex]);
    if (!seconds) return ParseError{Errors::kNotAnInteger};

    const auto now = ExpireCommand::Clock::now();
    const auto requested = std::chrono::duration<long double>(*seconds);
    const auto current_offset = std::chrono::duration<long double>(now.time_since_epoch());
    const auto maximum_offset =
        std::chrono::duration<long double>(ExpireCommand::TimePoint::max().time_since_epoch());
    const auto minimum_offset =
        std::chrono::duration<long double>(ExpireCommand::TimePoint::min().time_since_epoch());
    if (requested > maximum_offset - current_offset ||
        requested < minimum_offset - current_offset) {
        return ParseError{Errors::kInvalidExpireTime};
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
    if (arguments.empty()) return ParseError{Errors::kEmptyCommand};
    const std::string command = uppercase_ascii(arguments[kCommandIndex]);
    if (command == ExpireCommand::name) return parse_expire(arguments);

    const KeyCommandSpec* spec = find_key_command(command);
    if (!spec) return ParseError{Errors::kUnknownCommand};
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
    if (command == ExpireCommand::log_name && arguments.size() == 3) {
        auto deadline = parse_millisecond_deadline(arguments[2]);
        if (!deadline) return std::nullopt;
        return ExpireCommand{arguments[1], *deadline};
    }
    if (command == SetStateCommand::name && arguments.size() == 4) {
        if (arguments[3] == "PERSIST") return SetStateCommand{arguments[1], arguments[2], std::nullopt};
        auto deadline = parse_millisecond_deadline(arguments[3]);
        if (!deadline) return std::nullopt;
        return SetStateCommand{arguments[1], arguments[2], *deadline};
    }
    return std::nullopt;
}
