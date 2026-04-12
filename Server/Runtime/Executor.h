//
// Created by Mark on 4/10/26.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Commands/DeleteStatement.h"
#include "Commands/ExistsStatement.h"
#include "Commands/ExpireStatement.h"
#include "Commands/GetStatement.h"
#include "Commands/SetStatement.h"
#include "Runtime/ExecutionResult.h"
#include "Runtime/StorageEngine.h"

class Executor {
public:
    explicit Executor(StorageEngine& storage);

    ExecutionResult execute(Statement& statement);

private:
    template <typename V>
    ExecutionResult execute_set(const SetStatement<V>& statement);

    ExecutionResult execute_get(const GetStatement& statement);
    ExecutionResult execute_delete(const DeleteStatement& statement);
    ExecutionResult execute_exists(const ExistsStatement& statement);
    ExecutionResult execute_expire(const ExpireStatement& statement);
    StorageEngine& storage_;
};

#endif //EXECUTOR_H
