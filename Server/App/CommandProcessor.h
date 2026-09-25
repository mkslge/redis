#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include "Commands/Command.h"
#include "Commands/Executor.h"
#include "Core/Bytes.h"

#include <string>
#include <variant>

class ProcessedCommand {
public:
    Command command;
    ExecutionResult execution_result;
    bool mutating_command{false};
    bool should_log{false};
    Bytes aof_record;
};

struct ProcessError {
    std::string message;
};

// Outcome of one client command line: a processed command, or an error for a line
// that could not be parsed. A command that ran but failed is still a success here;
// see ExecutionResult::success.
class CommandProcessResult {
public:
    static CommandProcessResult success(ProcessedCommand processed_command);
    static CommandProcessResult failure(std::string error_message);

    bool is_success() const;
    const ProcessedCommand& processed_command() const;
    const std::string& error_message() const;

private:
    using Outcome = std::variant<ProcessedCommand, ProcessError>;

    explicit CommandProcessResult(Outcome outcome);

    Outcome outcome_;
};

class CommandProcessor {
public:
    explicit CommandProcessor(Executor& executor);

    CommandProcessResult process(const std::string& command_line) const;

private:
    CommandProcessResult process_command(Command command) const;
    Executor& executor_;
};

#endif //COMMANDPROCESSOR_H
