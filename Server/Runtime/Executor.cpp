//
// Created by Mark on 4/10/26.
//

#include "Executor.h"

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

ExecutionResult Executor::execute_set(const SetStatement& statement) {
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
            return execute_set(*dynamic_cast<SetStatement*>(&statement));
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
    const bool applied = storage_.expire_at(statement.key(), statement.expires_at());
    return ExecutionResult{
        .success = true,
        .message = "EXPIRE",
        .payload = applied
    };
}
