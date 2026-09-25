#include "App/CommandProcessor.h"

#include "Protocol/ArgumentSplitter.h"
#include "Commands/Parser.h"
#include "Protocol/RespCommandCodec.h"

#include <optional>
#include <string>
#include <vector>

CommandProcessResult CommandProcessResult::success(ProcessedCommand processed_command) {
    return CommandProcessResult(Outcome{
        std::in_place_type<ProcessedCommand>, std::move(processed_command)});
}

CommandProcessResult CommandProcessResult::failure(std::string error_message) {
    return CommandProcessResult(Outcome{
        std::in_place_type<CommandProcessError>, CommandProcessError{std::move(error_message)}});
}

bool CommandProcessResult::is_success() const {
    return std::holds_alternative<ProcessedCommand>(outcome_);
}

const ProcessedCommand& CommandProcessResult::processed_command() const {
    return std::get<ProcessedCommand>(outcome_);
}

const std::string& CommandProcessResult::error_message() const {
    return std::get<CommandProcessError>(outcome_).message;
}

CommandProcessResult::CommandProcessResult(Outcome outcome)
    : outcome_(std::move(outcome)) {}

CommandProcessor::CommandProcessor(Executor& executor) : executor_(executor) {}

CommandProcessResult CommandProcessor::process(const std::string& command_line) const {
    const std::optional<CommandArguments> arguments = ArgumentSplitter::split(command_line);
    if (!arguments.has_value()) {
        return CommandProcessResult::failure("unbalanced quotes in request");
    }

    ParseResult parsed = Parser::parse_request(*arguments);
    if (const auto* error = std::get_if<ParseError>(&parsed)) {
        return CommandProcessResult::failure(error->message);
    }

    return process_command(std::get<Command>(std::move(parsed)));
}

CommandProcessResult CommandProcessor::process_arguments(const CommandArguments& arguments) const {
    std::optional<Command> command = Parser::parse_arguments(arguments);
    if (!command.has_value()) return CommandProcessResult::failure("parse failure");
    return process_command(std::move(*command));
}

CommandProcessResult CommandProcessor::process_command(Command command) const {
    const ExecutionResult result = executor_.execute(command);
    const bool mutating_command = is_mutating(command);
    const bool should_log = result.success && mutating_command && result.did_mutate;
    Bytes aof_record;
    if (should_log) {
        if (std::holds_alternative<ExpireCommand>(command) || !result.aof_state) {
            aof_record = RespCommandCodec::encode(command_arguments(command));
        }
        if (result.aof_state) {
            aof_record += RespCommandCodec::encode(command_arguments(Command{*result.aof_state}));
        }
    }
    return CommandProcessResult::success(ProcessedCommand{
        .command = std::move(command),
        .execution_result = result,
        .mutating_command = mutating_command,
        .should_log = should_log,
        .aof_record = std::move(aof_record)
    });
}
