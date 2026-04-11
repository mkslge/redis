#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include "Interpreter/Runtime/Executor.h"

#include <string>

class CommandProcessor {
public:
    explicit CommandProcessor(Executor& executor);

    bool process_command(const std::string& command_line);

private:
    template <typename V>
    bool try_execute_set(Statement* statement);

    bool execute_statement(std::unique_ptr<Statement>& statement);

    Executor& executor_;
};

#endif //COMMANDPROCESSOR_H
