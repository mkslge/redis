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
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>


class Parser {
public:
    static std::unique_ptr<Statement> parse(std::vector<Token>& toks);

    static std::unique_ptr<GetStatement> try_parse_get(std::vector<Token>& toks) {
        if (toks.size() != kUnaryStatementTokenCount ||
            toks[kCommandTokenIndex].get_type() != TokenType::GET
            || !toks[kFirstArgumentTokenIndex].has_value()) {
            return nullptr;
        }

        auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
        if (!key.has_value()) {
            return nullptr;
        }

        return std::make_unique<GetStatement>(key.value());
    }

    template <typename V>
    static std::unique_ptr<SetStatement<V>> try_parse_set(std::vector<Token>& toks) {
        if (toks.size() != kBinaryStatementTokenCount ||
            toks[kCommandTokenIndex].get_type() != TokenType::SET
            || !toks[kFirstArgumentTokenIndex].has_value()
            || !toks[kSecondArgumentTokenIndex].has_value()) {
            return nullptr;
        }


        auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
        auto val = toks[kSecondArgumentTokenIndex].template get_prim<V>();
        if (!key.has_value() || !val.has_value()) {
            return nullptr;
        }

        return std::make_unique<SetStatement<V>>(key.value(), val.value());
    }

    static std::unique_ptr<DeleteStatement> try_parse_del(std::vector<Token>& toks) {
        if (toks.size() != kUnaryStatementTokenCount ||
            toks[kCommandTokenIndex].get_type() != TokenType::DEL
            || !toks[kFirstArgumentTokenIndex].has_value()) {
            return nullptr;
            }


        auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
        if (!key.has_value()) {
            return nullptr;
        }

        return std::make_unique<DeleteStatement>(key.value());
    }

    static std::unique_ptr<ExistsStatement> try_parse_exists(std::vector<Token>& toks) {
        if (toks.size() != kUnaryStatementTokenCount ||
            toks[kCommandTokenIndex].get_type() != TokenType::EXISTS
            || !toks[kFirstArgumentTokenIndex].has_value()) {
            return nullptr;
            }


        auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
        if (!key.has_value()) {
            return nullptr;
        }

        return std::make_unique<ExistsStatement>(key.value());
    }

    static std::unique_ptr<ExpireStatement> try_parse_expire(std::vector<Token>& toks) {
        if (toks.size() != kBinaryStatementTokenCount ||
            toks[kCommandTokenIndex].get_type() != TokenType::EXPIRE
            || toks[kSecondArgumentTokenIndex].get_type() != TokenType::INT
            || !toks[kFirstArgumentTokenIndex].has_value()
            ) {
            return nullptr;
            }


        auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
        auto time = toks[kSecondArgumentTokenIndex].template get_prim<int>();
        if (!key.has_value() || !time.has_value()) {
            return nullptr;
        }


        return std::make_unique<ExpireStatement>(key.value(), time.value());
    }

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

    template <typename Fn>
    static std::unique_ptr<Statement> dispatch_value_type(const Token& tok, Fn&& fn) {
        switch (tok.get_type()) {
            case TokenType::INT:
                return fn.template operator()<int>();
            case TokenType::DOUBLE:
                return fn.template operator()<double>();
            case TokenType::CHAR:
                return fn.template operator()<char>();
            case TokenType::STRING:
                return fn.template operator()<std::string>();
            default:
                return nullptr;
        }
    }

    static std::optional<Key> key_from_token(const Token& tok) {
        switch (tok.get_type()) {
            case TokenType::INT:
                return std::to_string(tok.template get_prim<int>().value());
            case TokenType::DOUBLE: {
                std::ostringstream stream;
                stream.precision(std::numeric_limits<double>::max_digits10);
                stream << tok.template get_prim<double>().value();
                return stream.str();
            }
            case TokenType::CHAR:
                return std::string(1, tok.template get_prim<char>().value());
            case TokenType::STRING:
                return tok.template get_prim<std::string>().value();
            default:
                return std::nullopt;
        }
    }
};



#endif //PARSER_H
