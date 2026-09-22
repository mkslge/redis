#include "Command.h"

#include <chrono>
#include <string>
namespace {
template<class... Visitors>
struct Overloaded : Visitors... { using Visitors::operator()...; };
template<class... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;
}

bool is_mutating(const Command& command) {
    return std::visit(Overloaded{
        [](const GetCommand&) { return false; },
        [](const SetCommand&) { return true; },
        [](const DeleteCommand&) { return true; },
        [](const ExistsCommand&) { return false; },
        [](const ExpireCommand&) { return true; },
        [](const TtlCommand&) { return false; },
        [](const PttlCommand&) { return false; },
        [](const PersistCommand&) { return true; }
    }, command);
}

CommandArguments command_arguments(const Command& command) {
    return std::visit(Overloaded{
        [](const GetCommand& value) -> CommandArguments { return {"GET", value.key}; },
        [](const SetCommand& value) -> CommandArguments { return {"SET", value.key, value.value}; },
        [](const DeleteCommand& value) -> CommandArguments { return {"DEL", value.key}; },
        [](const ExistsCommand& value) -> CommandArguments { return {"EXISTS", value.key}; },
        [](const ExpireCommand& value) -> CommandArguments {
            const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                value.expires_at.time_since_epoch()).count();
            return {"PEXPIREAT", value.key, std::to_string(milliseconds)};
        },
        [](const TtlCommand& value) -> CommandArguments { return {"TTL", value.key}; },
        [](const PttlCommand& value) -> CommandArguments { return {"PTTL", value.key}; },
        [](const PersistCommand& value) -> CommandArguments { return {"PERSIST", value.key}; }
    }, command);
}
