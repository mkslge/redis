#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include "Command.h"
#include "Executor.h"
#include "Bytes.h"

#include <optional>
#include <string>

class ProcessedCommand {
public:
    Command command;
    ExecutionResult execution_result;
    bool mutating_command{false};
    bool should_log{false};
    Bytes aof_record;
};

class CommandProcessResult {
public:
    static CommandProcessResult success(ProcessedCommand processed_command);
    static CommandProcessResult failure(std::string error_message);

    bool is_success() const;
    const ProcessedCommand& processed_command() const;
    const std::string& error_message() const;

private:
    explicit CommandProcessResult(std::optional<ProcessedCommand> processed_command,
                                  std::optional<std::string> error_message);

    std::optional<ProcessedCommand> processed_command_;
    std::optional<std::string> error_message_;
};

class CommandProcessor {
public:
    explicit CommandProcessor(Executor& executor);

    CommandProcessResult process(const std::string& command_line) const;
    CommandProcessResult process_arguments(const CommandArguments& arguments) const;

private:
    CommandProcessResult process_command(Command command) const;
    Executor& executor_;
};

#endif //COMMANDPROCESSOR_H
