//
// Created by Mark on 4/5/26.
//

#ifndef PARSER_H
#define PARSER_H

#include "Interpreter/Statements/Statement.h"
#include "Interpreter/Statements/GetStatement.h"
#include "Interpreter/Statements/SetStatement.h"
#include "Interpreter/Statements/DeleteStatement.h"
#include "Interpreter/Statements/ExistsStatement.h"
#include "Interpreter/Statements/ExpireStatement.h"
#include "Interpreter/Model/Key.h"
#include "Interpreter/Tokens/Token.h"
#include <memory>
#include <optional>
#include <vector>


class Parser {
public:
    static std::unique_ptr<Statement> parse(std::vector<Token>& toks);
    static std::unique_ptr<GetStatement> try_parse_get(std::vector<Token>& toks);

    template <typename V>
    static std::unique_ptr<SetStatement<V>> try_parse_set(std::vector<Token>& toks);
    static std::unique_ptr<DeleteStatement> try_parse_del(std::vector<Token>& toks);
    static std::unique_ptr<ExistsStatement> try_parse_exists(std::vector<Token>& toks);
    static std::unique_ptr<ExpireStatement> try_parse_expire(std::vector<Token>& toks);

private:
    static constexpr std::size_t kUnaryStatementTokenCount = 2;
    static constexpr std::size_t kBinaryStatementTokenCount = 3;
    static constexpr std::size_t kCommandTokenIndex = 0;
    static constexpr std::size_t kFirstArgumentTokenIndex = 1;
    static constexpr std::size_t kSecondArgumentTokenIndex = 2;

    static std::unique_ptr<Statement> parse_get_statement(std::vector<Token>& toks);
    static std::unique_ptr<Statement> parse_del_statement(std::vector<Token>& toks);
    static std::unique_ptr<Statement> parse_exists_statement(std::vector<Token>& toks);
    static std::unique_ptr<Statement> parse_set_statement(std::vector<Token>& toks);
    static std::unique_ptr<Statement> parse_expire_statement(std::vector<Token>& toks);
    static std::optional<Key> key_from_token(const Token& tok);
};



#endif //PARSER_H
