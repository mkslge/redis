#ifndef EXECUTIONRESULT_H
#define EXECUTIONRESULT_H

#include "Value.h"

#include <cstdint>
#include <string>
#include <variant>

using ExecutionPayload = std::variant<std::monostate, Value, bool, std::int64_t>;

class ExecutionResult {
public:
    bool success{true};
    bool did_mutate{false};
    std::string message;
    ExecutionPayload payload;
};

#endif //EXECUTIONRESULT_H
