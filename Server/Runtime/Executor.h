//
// Created by Mark on 4/10/26.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "DeleteStatement.h"
#include "ExistsStatement.h"
#include "ExpireStatement.h"
#include "GetStatement.h"
#include "SetStatement.h"
#include "ExecutionResult.h"
#include "StorageEngine.h"

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
