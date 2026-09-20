#include "ResponseFormatter.h"

#include "Value.h"

#include <string>

namespace {
std::string format_value(const Value& value) {
    return '"' + value.bytes() + '"';
}
} // namespace

std::string ResponseFormatter::format_error(const std::string& error_message) {
    return "ERROR " + error_message + "\n";
}

std::string ResponseFormatter::format_result(const StatementType statement_type, const ExecutionResult& result) {
    if (!result.success) {
        return format_error(result.message);
    }

    switch (statement_type) {
        case StatementType::GET: {
            std::string response = "GET";
            if (std::holds_alternative<Value>(result.payload)) {
                response += " value=" + format_value(std::get<Value>(result.payload));
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
            return format_error("unsupported statement");
    }
}
