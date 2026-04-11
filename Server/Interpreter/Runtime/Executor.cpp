//
// Created by Mark on 4/10/26.
//

#include "Interpreter/Runtime/Executor.h"

#include <iostream>
#include <string>

namespace {
template <typename T>
void print_value(const T& value) {
    std::cout << value;
}

template <>
void print_value<std::string>(const std::string& value) {
    std::cout << '"' << value << '"';
}

template <typename K>
void print_key_clause(const K& key) {
    std::cout << " key=";
    print_value(key);
}
} // namespace

Executor::Executor(StorageEngine& storage) : storage_(storage) {}

void Executor::execute_get(const GetStatement& statement) {
    const auto value = storage_.get(statement.key());
    std::cout << "GET";
    print_key_clause(statement.key());
    if (value.has_value()) {
        std::cout << " value=";
        value->visit([](const auto& stored_value) {
            print_value(stored_value);
        });
    }
    std::cout << std::endl;
}

template <typename V>
void Executor::execute_set(const SetStatement<V>& statement) {
    storage_.set(statement.key(), Value(statement.value()));
    std::cout << "SET";
    print_key_clause(statement.key());
    std::cout << " value=";
    print_value(statement.value());
    std::cout << std::endl;
}

void Executor::execute_delete(const DeleteStatement& statement) {
    const bool deleted = storage_.del(statement.key());
    std::cout << "DELETE";
    print_key_clause(statement.key());
    std::cout << " deleted=" << (deleted ? "true" : "false");
    std::cout << std::endl;
}

void Executor::execute_exists(const ExistsStatement& statement) {
    const bool exists = storage_.exists(statement.key());
    std::cout << "EXISTS";
    print_key_clause(statement.key());
    std::cout << " exists=" << (exists ? "true" : "false");
    std::cout << std::endl;
}

void Executor::execute_expire(const ExpireStatement& statement) {
    const bool applied = storage_.expire(statement.key(), std::chrono::seconds(statement.expire_time()));
    std::cout << "EXPIRE";
    print_key_clause(statement.key());
    std::cout << " ttl=" << statement.expire_time();
    std::cout << " applied=" << (applied ? "true" : "false");
    std::cout << std::endl;
}

template void Executor::execute_set<int>(const SetStatement<int>&);
template void Executor::execute_set<double>(const SetStatement<double>&);
template void Executor::execute_set<char>(const SetStatement<char>&);
template void Executor::execute_set<std::string>(const SetStatement<std::string>&);
