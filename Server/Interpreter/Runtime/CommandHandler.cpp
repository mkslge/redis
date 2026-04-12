#include "Interpreter/Runtime/CommandHandler.h"

#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
template <typename T>
std::string format_scalar(const T& value) {
    return std::to_string(value);
}

template <>
std::string format_scalar<std::string>(const std::string& value) {
    return '"' + value + '"';
}

template <>
std::string format_scalar<char>(const char& value) {
    return std::string(1, value);
}

std::string format_value(const Value& value) {
    return value.visit([](const auto& stored_value) {
        return format_scalar(stored_value);
    });
}
} // namespace

CommandHandler::CommandHandler(Executor& executor) : executor_(executor) {}

CommandResponse CommandHandler::process_command(const std::string& command_line) const {
    const std::optional<std::vector<Token>> tokens = Tokenizer::tokenize(command_line);
    if (!tokens.has_value()) {
        return CommandResponse{
            .response = "ERROR invalid command\n",
            .should_log = false,
            .log_entry = ""
        };
    }

    std::vector<Token> parsed_tokens = tokens.value();
    std::unique_ptr<Statement> statement = Parser::parse(parsed_tokens);
    if (statement == nullptr) {
        return CommandResponse{
            .response = "ERROR parse failure\n",
            .should_log = false,
            .log_entry = ""
        };
    }

    const ExecutionResult result = executor_.execute(*statement);
    return CommandResponse{
        .response = format_result(*statement, result),
        .should_log = result.success && statement->mutates(),
        .log_entry = result.success && statement->mutates() ? statement->to_string() : ""
    };
}

std::string CommandHandler::format_result(const Statement& statement, const ExecutionResult& result) {
    if (!result.success) {
        return "ERROR " + result.message + "\n";
    }

    switch (statement.get_type()) {
        case StatementType::GET: {
            std::string response = "GET";
            const auto& payload = result.payload;
            if (std::holds_alternative<Value>(payload)) {
                response += " value=" + format_value(std::get<Value>(payload));
            }
            return response + "\n";
        }
        case StatementType::SET:
            return "SET value=" + format_value(std::get<Value>(result.payload)) + "\n";
        case StatementType::DELETE:
            return "DELETE deleted=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        case StatementType::EXISTS:
            return "EXISTS exists=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        case StatementType::EXPIRE:
            return "EXPIRE applied=" + std::string(std::get<bool>(result.payload) ? "true" : "false") + "\n";
        default:
            return "ERROR unsupported statement\n";
    }
}
