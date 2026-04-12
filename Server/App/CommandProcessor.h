#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include "Commands/StatementType.h"
#include "Runtime/Executor.h"

#include <optional>
#include <string>

class ProcessedCommand {
public:
    StatementType statement_type;
    ExecutionResult execution_result;
    bool should_log{false};
    std::string log_entry;
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

private:
    Executor& executor_;
};

#endif //COMMANDPROCESSOR_H
