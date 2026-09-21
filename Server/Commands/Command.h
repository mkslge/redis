#ifndef COMMAND_H
#define COMMAND_H

#include "Bytes.h"
#include "Key.h"

#include <chrono>
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

using Command = std::variant<GetCommand, SetCommand, DeleteCommand, ExistsCommand,
                             ExpireCommand, TtlCommand, PttlCommand, PersistCommand>;

bool is_mutating(const Command& command);
CommandArguments command_arguments(const Command& command);

#endif //COMMAND_H
