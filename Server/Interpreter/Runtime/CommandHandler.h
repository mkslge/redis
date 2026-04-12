#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Statements/StatementType.h"

#include <string>

class CommandResponse {
public:
    bool success{true};
    std::string error_message;
    StatementType statement_type;
    ExecutionResult execution_result;
    bool should_log{false};
    std::string log_entry;
};

class CommandHandler {
public:
    explicit CommandHandler(Executor& executor);

    CommandResponse process_command(const std::string& command_line) const;

    Executor& executor_;
};

#endif //COMMANDHANDLER_H
