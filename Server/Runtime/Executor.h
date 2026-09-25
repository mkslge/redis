#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Command.h"
#include "ExecutionResult.h"
#include "StorageEngine.h"

class Executor {
public:
    explicit Executor(StorageEngine& storage);
    ExecutionResult execute(const Command& command);

private:
    ExecutionResult execute_command(const GetCommand& command);
    ExecutionResult execute_command(const SetCommand& command);
    ExecutionResult execute_command(const DeleteCommand& command);
    ExecutionResult execute_command(const ExistsCommand& command);
    ExecutionResult execute_command(const ExpireCommand& command);
    ExecutionResult execute_command(const TtlCommand& command);
    ExecutionResult execute_command(const PttlCommand& command);
    ExecutionResult execute_command(const PersistCommand& command);
    ExecutionResult execute_command(const IncrCommand& command);
    ExecutionResult execute_command(const DecrCommand& command);
    ExecutionResult execute_command(const IncrByCommand& command);
    ExecutionResult execute_command(const DecrByCommand& command);
    ExecutionResult execute_command(const SetStateCommand& command);

    ExecutionResult adjust_integer(const Key& key, std::int64_t amount, bool subtract);
    ExecutionResult adjust_integer(const Key& key, const Bytes& amount, bool subtract);
    ExecutionResult integer_result(const Key& key, const StorageEngine::IntegerResult& result);

    StorageEngine& storage_;
};

#endif //EXECUTOR_H
