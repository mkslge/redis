//
// Created by Mark on 4/10/26.
//

#include "Runtime/Executor.h"

#include <string>

Executor::Executor(StorageEngine& storage) : storage_(storage) {}

ExecutionResult Executor::execute_get(const GetStatement& statement) {
    const auto value = storage_.get(statement.key());
    if (value.has_value()) {
        return ExecutionResult{
            .success = true,
            .message = "GET",
            .payload = value.value()
        };
    }

    return ExecutionResult{
        .success = true,
        .message = "GET",
        .payload = std::monostate{}
    };
}

template <typename V>
ExecutionResult Executor::execute_set(const SetStatement<V>& statement) {
    storage_.set(statement.key(), Value(statement.value()));
    return ExecutionResult{
        .success = true,
        .message = "SET",
        .payload = Value(statement.value())
    };
}

ExecutionResult Executor::execute(Statement& statement) {
    switch (statement.get_type()) {
        case StatementType::GET:
            return execute_get(*dynamic_cast<GetStatement*>(&statement));
        case StatementType::SET:
            if (auto* int_statement = dynamic_cast<SetStatement<int>*>(&statement); int_statement != nullptr) {
                return execute_set(*int_statement);
            }
            if (auto* double_statement = dynamic_cast<SetStatement<double>*>(&statement); double_statement != nullptr) {
                return execute_set(*double_statement);
            }
            if (auto* char_statement = dynamic_cast<SetStatement<char>*>(&statement); char_statement != nullptr) {
                return execute_set(*char_statement);
            }
            if (auto* string_statement = dynamic_cast<SetStatement<std::string>*>(&statement); string_statement != nullptr) {
                return execute_set(*string_statement);
            }
            return ExecutionResult{
                .success = false,
                .message = "Unsupported SET value type",
                .payload = std::monostate{}
            };
        case StatementType::DELETE:
            return execute_delete(*dynamic_cast<DeleteStatement*>(&statement));
        case StatementType::EXISTS:
            return execute_exists(*dynamic_cast<ExistsStatement*>(&statement));
        case StatementType::EXPIRE:
            return execute_expire(*dynamic_cast<ExpireStatement*>(&statement));
        default:
            return ExecutionResult{
                .success = false,
                .message = "Unsupported statement type",
                .payload = std::monostate{}
            };
    }
}

ExecutionResult Executor::execute_delete(const DeleteStatement& statement) {
    const bool deleted = storage_.del(statement.key());
    return ExecutionResult{
        .success = true,
        .message = "DELETE",
        .payload = deleted
    };
}

ExecutionResult Executor::execute_exists(const ExistsStatement& statement) {
    const bool exists = storage_.exists(statement.key());
    return ExecutionResult{
        .success = true,
        .message = "EXISTS",
        .payload = exists
    };
}

ExecutionResult Executor::execute_expire(const ExpireStatement& statement) {
    const bool applied = storage_.expire(statement.key(), std::chrono::seconds(statement.expire_time()));
    return ExecutionResult{
        .success = true,
        .message = "EXPIRE",
        .payload = applied
    };
}

template ExecutionResult Executor::execute_set<int>(const SetStatement<int>&);
template ExecutionResult Executor::execute_set<double>(const SetStatement<double>&);
template ExecutionResult Executor::execute_set<char>(const SetStatement<char>&);
template ExecutionResult Executor::execute_set<std::string>(const SetStatement<std::string>&);
