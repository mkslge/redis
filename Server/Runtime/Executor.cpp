#include "Executor.h"

Executor::Executor(StorageEngine& storage) : storage_(storage) {}

ExecutionResult Executor::execute(const Command& command) {
    return std::visit([this](const auto& concrete) {
        return execute_command(concrete);
    }, command);
}

ExecutionResult Executor::execute_command(const GetCommand& command) {
    const auto value = storage_.get(command.key);
    return {.success = true,
            .payload = value.has_value() ? ExecutionPayload{*value}
                                         : ExecutionPayload{std::monostate{}}};
}

ExecutionResult Executor::execute_command(const SetCommand& command) {
    storage_.set(command.key, Value(command.value));
    return {.success = true, .did_mutate = true, .payload = Value(command.value)};
}

ExecutionResult Executor::execute_command(const DeleteCommand& command) {
    const bool deleted = storage_.del(command.key);
    return {.success = true, .did_mutate = deleted, .payload = deleted};
}

ExecutionResult Executor::execute_command(const ExistsCommand& command) {
    return {.success = true, .payload = storage_.exists(command.key)};
}

ExecutionResult Executor::execute_command(const ExpireCommand& command) {
    const bool applied = storage_.expire_at(command.key, command.expires_at);
    return {.success = true, .did_mutate = applied, .payload = applied};
}

ExecutionResult Executor::execute_command(const TtlCommand& command) {
    std::int64_t ttl = storage_.ttl_milliseconds(command.key);
    if (ttl >= 0) ttl = ttl / 1000 + (ttl % 1000 >= 500 ? 1 : 0);
    return {.success = true, .payload = ttl};
}

ExecutionResult Executor::execute_command(const PttlCommand& command) {
    return {.success = true, .payload = storage_.ttl_milliseconds(command.key)};
}

ExecutionResult Executor::execute_command(const PersistCommand& command) {
    const bool removed = storage_.persist(command.key);
    return {.success = true, .did_mutate = removed, .payload = removed};
}
