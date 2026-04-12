#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include "Interpreter/Runtime/Executor.h"

#include <string>

class CommandResponse {
public:
    std::string response;
    bool should_log{false};
    std::string log_entry;
};

class CommandHandler {
public:
    explicit CommandHandler(Executor& executor);

    CommandResponse process_command(const std::string& command_line) const;

private:
    static std::string format_result(const Statement& statement, const ExecutionResult& result);

    Executor& executor_;
};

#endif //COMMANDHANDLER_H
