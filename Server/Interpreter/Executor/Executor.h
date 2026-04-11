//
// Created by Mark on 4/10/26.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Interpreter/Executor/Key.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"

class Executor {
public:
    static void execute_get(const GetStatement& statement);

    template <typename V>
    static void execute_set(const SetStatement<V>& statement);

    static void execute_delete(const DeleteStatement& statement);

    static void execute_exists(const ExistsStatement& statement);

    static void execute_expire(const ExpireStatement& statement);
};

#endif //EXECUTOR_H
