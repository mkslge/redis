#include "Interpreter/Runtime/CommandProcessor.h"

#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"

#include <iostream>
#include <memory>
#include <vector>

CommandProcessor::CommandProcessor(Executor& executor) : executor_(executor) {}

bool CommandProcessor::process_command(const std::string& command_line) {
    auto tokens = Tokenizer::tokenize(command_line);
    if (!tokens.has_value()) {
        std::cout << "ERROR invalid command" << std::endl;
        return false;
    }

    std::vector<Token> parsed_tokens = tokens.value();
    auto statement = Parser::parse(parsed_tokens);
    if (statement == nullptr) {
        std::cout << "ERROR parse failure" << std::endl;
        return false;
    }

    if (!execute_statement(statement)) {
        std::cout << "ERROR unsupported statement" << std::endl;
        return false;
    }

    return true;
}

template <typename V>
bool CommandProcessor::try_execute_set(Statement* statement) {
    auto* set_statement = dynamic_cast<SetStatement<V>*>(statement);
    if (set_statement == nullptr) {
        return false;
    }

    executor_.execute_set(*set_statement);
    return true;
}

bool CommandProcessor::execute_statement(std::unique_ptr<Statement>& statement) {
    switch (statement->get_type()) {
        case StatementType::GET:
            executor_.execute_get(*dynamic_cast<GetStatement*>(statement.get()));
            return true;
        case StatementType::SET:
            return try_execute_set<int>(statement.get()) ||
                   try_execute_set<double>(statement.get()) ||
                   try_execute_set<char>(statement.get()) ||
                   try_execute_set<std::string>(statement.get());
        case StatementType::DELETE:
            executor_.execute_delete(*dynamic_cast<DeleteStatement*>(statement.get()));
            return true;
        case StatementType::EXISTS:
            executor_.execute_exists(*dynamic_cast<ExistsStatement*>(statement.get()));
            return true;
        case StatementType::EXPIRE:
            executor_.execute_expire(*dynamic_cast<ExpireStatement*>(statement.get()));
            return true;
        default:
            return false;
    }
}

template bool CommandProcessor::try_execute_set<int>(Statement*);
template bool CommandProcessor::try_execute_set<double>(Statement*);
template bool CommandProcessor::try_execute_set<char>(Statement*);
template bool CommandProcessor::try_execute_set<std::string>(Statement*);
