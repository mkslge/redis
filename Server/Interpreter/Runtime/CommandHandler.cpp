#include "Interpreter/Runtime/CommandHandler.h"

#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

CommandHandler::CommandHandler(Executor& executor) : executor_(executor) {}

CommandResponse CommandHandler::process_command(const std::string& command_line) const {
    const std::optional<std::vector<Token>> tokens = Tokenizer::tokenize(command_line);
    if (!tokens.has_value()) {
        return CommandResponse{
            .success = false,
            .error_message = "invalid command",
            .statement_type = StatementType::GET,
            .execution_result = ExecutionResult{},
            .should_log = false,
            .log_entry = ""
        };
    }

    std::vector<Token> parsed_tokens = tokens.value();
    std::unique_ptr<Statement> statement = Parser::parse(parsed_tokens);
    if (statement == nullptr) {
        return CommandResponse{
            .success = false,
            .error_message = "parse failure",
            .statement_type = StatementType::GET,
            .execution_result = ExecutionResult{},
            .should_log = false,
            .log_entry = ""
        };
    }

    const ExecutionResult result = executor_.execute(*statement);
    return CommandResponse{
        .success = true,
        .error_message = "",
        .statement_type = statement->get_type(),
        .execution_result = result,
        .should_log = result.success && statement->mutates(),
        .log_entry = result.success && statement->mutates() ? statement->to_string() : ""
    };
}
