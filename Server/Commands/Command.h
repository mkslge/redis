#ifndef COMMAND_H
#define COMMAND_H

#include "Core/Bytes.h"
#include "Core/CommandArguments.h"
#include "Core/Key.h"

#include <chrono>
#include <optional>
#include <string_view>
#include <variant>

// Each command states its wire name and whether it changes the keyspace.
struct GetCommand {
    static constexpr std::string_view name = "GET";
    static constexpr bool mutating = false;
    Key key;
};
struct SetCommand {
    static constexpr std::string_view name = "SET";
    static constexpr bool mutating = true;
    Key key;
    Bytes value;
};
struct DeleteCommand {
    static constexpr std::string_view name = "DEL";
    static constexpr bool mutating = true;
    Key key;
};
struct ExistsCommand {
    static constexpr std::string_view name = "EXISTS";
    static constexpr bool mutating = false;
    Key key;
};
// Parsed from a relative EXPIRE; stored and logged (as PEXPIREAT) with an absolute deadline.
struct ExpireCommand {
    static constexpr std::string_view name = "EXPIRE";
    static constexpr std::string_view log_name = "PEXPIREAT";
    static constexpr bool mutating = true;
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    Key key;
    TimePoint expires_at;
};
struct TtlCommand {
    static constexpr std::string_view name = "TTL";
    static constexpr bool mutating = false;
    Key key;
};
struct PttlCommand {
    static constexpr std::string_view name = "PTTL";
    static constexpr bool mutating = false;
    Key key;
};
struct PersistCommand {
    static constexpr std::string_view name = "PERSIST";
    static constexpr bool mutating = true;
    Key key;
};
struct IncrCommand {
    static constexpr std::string_view name = "INCR";
    static constexpr bool mutating = true;
    Key key;
};
struct DecrCommand {
    static constexpr std::string_view name = "DECR";
    static constexpr bool mutating = true;
    Key key;
};
struct IncrByCommand {
    static constexpr std::string_view name = "INCRBY";
    static constexpr bool mutating = true;
    Key key;
    Bytes amount;
};
struct DecrByCommand {
    static constexpr std::string_view name = "DECRBY";
    static constexpr bool mutating = true;
    Key key;
    Bytes amount;
};
// Internal AOF command: one complete value and its absolute expiration.
struct SetStateCommand {
    static constexpr std::string_view name = "SETSTATE";
    static constexpr bool mutating = true;
    Key key;
    Bytes value;
    std::optional<ExpireCommand::TimePoint> expires_at;
};

using Command = std::variant<GetCommand, SetCommand, DeleteCommand, ExistsCommand,
                             ExpireCommand, TtlCommand, PttlCommand, PersistCommand,
                             IncrCommand, DecrCommand, IncrByCommand, DecrByCommand,
                             SetStateCommand>;

bool is_mutating(const Command& command);
// The command's arguments as written to the AOF.
CommandArguments command_arguments(const Command& command);

#endif //COMMAND_H
