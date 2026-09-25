#include "App/CommandProcessor.h"

#include "Protocol/ArgumentSplitter.h"
#include "Commands/Errors.h"
#include "Commands/Parser.h"
#include "Persistence/AofRecords.h"

#include <optional>
#include <string>
#include <vector>

CommandProcessResult CommandProcessResult::success(ProcessedCommand processed_command) {
    return CommandProcessResult(Outcome{
        std::in_place_type<ProcessedCommand>, std::move(processed_command)});
}

CommandProcessResult CommandProcessResult::failure(std::string error_message) {
    return CommandProcessResult(Outcome{
        std::in_place_type<ProcessError>, ProcessError{std::move(error_message)}});
}

bool CommandProcessResult::is_success() const {
    return std::holds_alternative<ProcessedCommand>(outcome_);
}

const ProcessedCommand& CommandProcessResult::processed_command() const {
    return std::get<ProcessedCommand>(outcome_);
}

const std::string& CommandProcessResult::error_message() const {
    return std::get<ProcessError>(outcome_).message;
}

CommandProcessResult::CommandProcessResult(Outcome outcome)
    : outcome_(std::move(outcome)) {}

CommandProcessor::CommandProcessor(Executor& executor) : executor_(executor) {}

CommandProcessResult CommandProcessor::process(const std::string& command_line) const {
    const std::optional<CommandArguments> arguments = ArgumentSplitter::split(command_line);
    if (!arguments.has_value()) {
        return CommandProcessResult::failure(Errors::kUnbalancedQuotes);
    }

    ParseResult parsed = Parser::parse_request(*arguments);
    if (const auto* error = std::get_if<ParseError>(&parsed)) {
        return CommandProcessResult::failure(error->message);
    }

    return process_command(std::get<Command>(std::move(parsed)));
}

CommandProcessResult CommandProcessor::process_command(Command command) const {
    const ExecutionResult result = executor_.execute(command);
    const bool mutating_command = is_mutating(command);
    Bytes aof_record = aof_records_for(command, result);
    const bool should_log = !aof_record.empty();
    return CommandProcessResult::success(ProcessedCommand{
        .command = std::move(command),
        .execution_result = result,
        .mutating_command = mutating_command,
        .should_log = should_log,
        .aof_record = std::move(aof_record)
    });
}
