#ifndef EXECUTIONRESULT_H
#define EXECUTIONRESULT_H

#include "Commands/Command.h"
#include "Core/Value.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

using ExecutionPayload = std::variant<std::monostate, Value, bool, std::int64_t>;

class ExecutionResult {
public:
    bool success{true};
    bool did_mutate{false};
    std::string message;
    ExecutionPayload payload;
    // The key's full state after a mutation, when the executor knows it.
    std::optional<SetStateCommand> resulting_state;
};

#endif //EXECUTIONRESULT_H
