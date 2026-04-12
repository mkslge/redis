#include "Interpreter/Runtime/CommandProcessor.h"

#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

CommandProcessResult CommandProcessResult::success(ProcessedCommand processed_command) {
    return CommandProcessResult(std::move(processed_command), std::nullopt);
}

CommandProcessResult CommandProcessResult::failure(std::string error_message) {
    return CommandProcessResult(std::nullopt, std::move(error_message));
}

bool CommandProcessResult::is_success() const {
    return processed_command_.has_value();
}

const ProcessedCommand& CommandProcessResult::processed_command() const {
    return processed_command_.value();
}

const std::string& CommandProcessResult::error_message() const {
    return error_message_.value();
}

CommandProcessResult::CommandProcessResult(std::optional<ProcessedCommand> processed_command,
                                           std::optional<std::string> error_message)
    : processed_command_(std::move(processed_command)), error_message_(std::move(error_message)) {}

CommandProcessor::CommandProcessor(Executor& executor) : executor_(executor) {}

CommandProcessResult CommandProcessor::process(const std::string& command_line) const {
    const std::optional<std::vector<Token>> tokens = Tokenizer::tokenize(command_line);
    if (!tokens.has_value()) {
        return CommandProcessResult::failure("invalid command");
    }

    std::vector<Token> parsed_tokens = tokens.value();
    std::unique_ptr<Statement> statement = Parser::parse(parsed_tokens);
    if (statement == nullptr) {
        return CommandProcessResult::failure("parse failure");
    }

    const ExecutionResult result = executor_.execute(*statement);
    return CommandProcessResult::success(ProcessedCommand{
        .statement_type = statement->get_type(),
        .execution_result = result,
        .should_log = result.success && statement->mutates(),
        .log_entry = result.success && statement->mutates() ? statement->to_string() : ""
    });
}
