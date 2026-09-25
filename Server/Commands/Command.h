#ifndef COMMAND_H
#define COMMAND_H

#include "Core/Bytes.h"
#include "Core/Key.h"

#include <chrono>
#include <optional>
#include <variant>

struct GetCommand { Key key; };
struct SetCommand { Key key; Bytes value; };
struct DeleteCommand { Key key; };
struct ExistsCommand { Key key; };
struct ExpireCommand {
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    Key key;
    TimePoint expires_at;
};
struct TtlCommand { Key key; };
struct PttlCommand { Key key; };
struct PersistCommand { Key key; };
struct IncrCommand { Key key; };
struct DecrCommand { Key key; };
struct IncrByCommand { Key key; Bytes amount; };
struct DecrByCommand { Key key; Bytes amount; };
// Internal AOF command: one complete value and its absolute expiration.
struct SetStateCommand {
    Key key;
    Bytes value;
    std::optional<ExpireCommand::TimePoint> expires_at;
};

using Command = std::variant<GetCommand, SetCommand, DeleteCommand, ExistsCommand,
                             ExpireCommand, TtlCommand, PttlCommand, PersistCommand,
                             IncrCommand, DecrCommand, IncrByCommand, DecrByCommand,
                             SetStateCommand>;

bool is_mutating(const Command& command);
CommandArguments command_arguments(const Command& command);

#endif //COMMAND_H
