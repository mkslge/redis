#ifndef EXECUTIONRESULT_H
#define EXECUTIONRESULT_H

#include "Interpreter/Model/Value.h"

#include <string>
#include <variant>

using ExecutionPayload = std::variant<std::monostate, Value, bool>;

class ExecutionResult {
public:
    bool success{true};
    std::string message;
    ExecutionPayload payload;
};

#endif //EXECUTIONRESULT_H
