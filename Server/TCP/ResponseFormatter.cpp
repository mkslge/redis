#include "TCP/ResponseFormatter.h"

#include "Interpreter/Model/Value.h"

#include <string>

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
