#include "Commands/Command.h"

#include "Core/Overloaded.h"

#include <chrono>
#include <string>

namespace {
std::string unix_milliseconds(const ExpireCommand::TimePoint time_point) {
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()).count());
}
}

bool is_mutating(const Command& command) {
    return std::visit([](const auto& concrete) {
        return std::remove_cvref_t<decltype(concrete)>::mutating;
    }, command);
}

CommandArguments command_arguments(const Command& command) {
    return std::visit(Overloaded{
        [](const SetCommand& value) -> CommandArguments {
            return {Bytes(SetCommand::name), value.key, value.value};
        },
        [](const OneOf<IncrByCommand, DecrByCommand> auto& value) -> CommandArguments {
            return {Bytes(value.name), value.key, value.amount};
        },
        [](const ExpireCommand& value) -> CommandArguments {
            return {Bytes(ExpireCommand::log_name), value.key, unix_milliseconds(value.expires_at)};
        },
        [](const SetStateCommand& value) -> CommandArguments {
            return {Bytes(SetStateCommand::name), value.key, value.value,
                    value.expires_at ? unix_milliseconds(*value.expires_at) : "PERSIST"};
        },
        [](const OneOf<GetCommand, DeleteCommand, ExistsCommand, TtlCommand, PttlCommand,
                       PersistCommand, IncrCommand, DecrCommand> auto& value) -> CommandArguments {
            return {Bytes(value.name), value.key};
        }
    }, command);
}
