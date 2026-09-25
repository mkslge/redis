#include "Commands/Executor.h"

#include "Commands/Errors.h"

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
    const auto result = storage_.expire_at(command.key, command.expires_at);
    return {.success = true, .did_mutate = result.applied, .payload = result.applied,
            .resulting_state = result.value
                ? std::optional<SetStateCommand>{SetStateCommand{
                    command.key, result.value->bytes(), command.expires_at}}
                : std::nullopt};
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
    const auto value = storage_.persist(command.key);
    const bool removed = value.has_value();
    return {.success = true, .did_mutate = removed, .payload = removed,
            .resulting_state = removed
                ? std::optional<SetStateCommand>{SetStateCommand{command.key, value->bytes(), std::nullopt}}
                : std::nullopt};
}

ExecutionResult Executor::adjust_integer(const Key& key, const std::int64_t amount,
                                         const bool subtract) {
    return integer_result(key, storage_.adjust_integer(key, amount, subtract));
}

ExecutionResult Executor::adjust_integer(const Key& key, const Bytes& amount,
                                         const bool subtract) {
    return integer_result(key, storage_.adjust_integer(key, amount, subtract));
}

ExecutionResult Executor::integer_result(const Key& key,
                                         const StorageEngine::IntegerResult& result) {
    if (result.error == StorageEngine::IntegerError::INVALID_INTEGER)
        return {.success = false, .message = Errors::kNotAnInteger};
    if (result.error == StorageEngine::IntegerError::WOULD_OVERFLOW)
        return {.success = false, .message = Errors::kIntegerOverflow};
    return {.success = true, .did_mutate = true, .payload = result.value,
            .resulting_state = SetStateCommand{key, std::to_string(result.value), result.expires_at}};
}

ExecutionResult Executor::execute_command(const IncrCommand& command) {
    return adjust_integer(command.key, 1, false);
}

ExecutionResult Executor::execute_command(const DecrCommand& command) {
    return adjust_integer(command.key, 1, true);
}

ExecutionResult Executor::execute_command(const IncrByCommand& command) {
    return adjust_integer(command.key, command.amount, false);
}

ExecutionResult Executor::execute_command(const DecrByCommand& command) {
    return adjust_integer(command.key, command.amount, true);
}

ExecutionResult Executor::execute_command(const SetStateCommand& command) {
    storage_.restore_state(command.key, Value(command.value), command.expires_at);
    return {.success = true, .did_mutate = true};
}
