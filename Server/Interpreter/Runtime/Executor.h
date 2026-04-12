//
// Created by Mark on 4/10/26.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Interpreter/Runtime/ExecutionResult.h"
#include "Interpreter/Runtime/StorageEngine.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"

class Executor {
public:
    explicit Executor(StorageEngine& storage);

    ExecutionResult execute(Statement& statement);

private:
    template <typename V>
    ExecutionResult execute_set(const SetStatement<V>& statement);

    template <typename V>
    ExecutionResult try_execute_set(Statement* statement);

    ExecutionResult execute_get(const GetStatement& statement);
    ExecutionResult execute_delete(const DeleteStatement& statement);
    ExecutionResult execute_exists(const ExistsStatement& statement);
    ExecutionResult execute_expire(const ExpireStatement& statement);
    StorageEngine& storage_;
};

#endif //EXECUTOR_H
