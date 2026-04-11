//
// Created by Mark on 4/10/26.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Interpreter/Runtime/StorageEngine.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"

class Executor {
public:
    explicit Executor(StorageEngine& storage);

    bool execute(Statement& statement);

private:
    template <typename V>
    void execute_set(const SetStatement<V>& statement);

    template <typename V>
    bool try_execute_set(Statement* statement);

    void execute_get(const GetStatement& statement);
    void execute_delete(const DeleteStatement& statement);
    void execute_exists(const ExistsStatement& statement);
    void execute_expire(const ExpireStatement& statement);
    StorageEngine& storage_;
};

#endif //EXECUTOR_H
