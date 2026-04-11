//
// Created by Mark on 4/10/26.
//

#include "Interpreter/Executor/Executor.h"

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

void Executor::execute_get(const GetStatement& statement) {
    std::cout << "GET";
    print_key_clause(statement.key());
    std::cout << std::endl;
}

template <typename V>
void Executor::execute_set(const SetStatement<V>& statement) {
    std::cout << "SET";
    print_key_clause(statement.key());
    std::cout << " value=";
    print_value(statement.value());
    std::cout << std::endl;
}

void Executor::execute_delete(const DeleteStatement& statement) {
    std::cout << "DELETE";
    print_key_clause(statement.key());
    std::cout << std::endl;
}

void Executor::execute_exists(const ExistsStatement& statement) {
    std::cout << "EXISTS";
    print_key_clause(statement.key());
    std::cout << std::endl;
}

void Executor::execute_expire(const ExpireStatement& statement) {
    std::cout << "EXPIRE";
    print_key_clause(statement.key());
    std::cout << " ttl=" << statement.expire_time() << std::endl;
}

template void Executor::execute_set<int>(const SetStatement<int>&);
template void Executor::execute_set<double>(const SetStatement<double>&);
template void Executor::execute_set<char>(const SetStatement<char>&);
template void Executor::execute_set<std::string>(const SetStatement<std::string>&);
