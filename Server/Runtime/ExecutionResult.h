#ifndef EXECUTIONRESULT_H
#define EXECUTIONRESULT_H

#include "Command.h"
#include "Value.h"

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
    std::optional<SetStateCommand> aof_state;
};

#endif //EXECUTIONRESULT_H
