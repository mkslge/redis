#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include "Interpreter/Runtime/Executor.h"

#include <string>

class CommandHandler {
public:
    explicit CommandHandler(Executor& executor);

    std::string process_command(const std::string& command_line) const;

private:
    static std::string format_result(const Statement& statement, const ExecutionResult& result);

    Executor& executor_;
};

#endif //COMMANDHANDLER_H
