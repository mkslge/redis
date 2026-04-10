//
// Created by Mark on 4/5/26.
//

#ifndef PARSER_H
#define PARSER_H

#include "Statement.h"
#include "StatementType.h"
#include "GetStatement.h"
#include "SetStatement.h"
#include "DeleteStatement.h"
#include "ExistsStatement.h"
#include "ExpireStatement.h"
#include "Token.h"
#include "PrimToken.h"
#include <memory>
#include <optional>


class Parser {
public:
    static std::optional<std::unique_ptr<Statement>> parse(std::vector<Token>& toks);

    template <typename K>
    static std::optional<std::unique_ptr<GetStatement<K>>> try_parse_get(std::vector<Token>& toks) {
        if (toks.size() != 2 ||
            toks[0].get_type() != TokenType::GET
            || !toks[1].is_prim()) {
            return std::nullopt;
        }


        auto key = toks[1].template get_prim<K>();
        if (!key.has_value()) {
            return std::nullopt;
        }

        return std::make_unique<GetStatement<K>>(key.value());
    }

    template <typename K, typename V>
    static std::optional<std::unique_ptr<SetStatement<K, V>>> try_parse_set(std::vector<Token>& toks) {
        if (toks.size() != 3 ||
            toks[0].get_type() != TokenType::SET
            || !toks[1].is_prim()
            || !toks[2].is_prim()) {
            return std::nullopt;
        }


        auto key = toks[1].template get_prim<K>();
        auto val = toks[2].template get_prim<V>();
        if (!key.has_value() || !val.has_value()) {
            return std::nullopt;
        }

        return std::make_unique<SetStatement<K, V>>(key.value(), val.value());
    }

    template <typename K>
    static std::optional<std::unique_ptr<DeleteStatement<K>>> try_parse_del(std::vector<Token>& toks) {
        if (toks.size() != 2 ||
            toks[0].get_type() != TokenType::DEL
            || !toks[1].is_prim()) {
            return std::nullopt;
            }


        auto key = toks[1].template get_prim<K>();
        if (!key.has_value()) {
            return std::nullopt;
        }

        return std::make_unique<DeleteStatement<K>>(key.value());
    }

    template <typename K>
    static std::optional<std::unique_ptr<ExistsStatement<K>>> try_parse_exists(std::vector<Token>& toks) {
        if (toks.size() != 2 ||
            toks[0].get_type() != TokenType::EXISTS
            || !toks[1].is_prim()) {
            return std::nullopt;
            }


        auto key = toks[1].template get_prim<K>();
        if (!key.has_value()) {
            return std::nullopt;
        }

        return std::make_unique<ExistsStatement<K>>(key.value());
    }

    template <typename K>
    static std::optional<std::unique_ptr<ExpireStatement<K>>> try_parse_expire(std::vector<Token>& toks) {
        if (toks.size() != 3 ||
            toks[0].get_type() != TokenType::EXPIRE
            || toks[2].get_type() != TokenType::INT
            || !toks[1].is_prim()
            ) {
            return std::nullopt;
            }


        auto key = toks[1].template get_prim<K>();
        auto time = toks[2].template get_prim<int>();
        if (!key.has_value() || !time.has_value()) {
            return std::nullopt;
        }


        return std::make_unique<ExpireStatement<K>>(key.value(), time.value());
    }

private:
    static std::optional<std::unique_ptr<Statement>> parse_get_statement(std::vector<Token>& toks);
    static std::optional<std::unique_ptr<Statement>> parse_del_statement(std::vector<Token>& toks);
    static std::optional<std::unique_ptr<Statement>> parse_exists_statement(std::vector<Token>& toks);
    static std::optional<std::unique_ptr<Statement>> parse_set_statement(std::vector<Token>& toks);
    static std::optional<std::unique_ptr<Statement>> parse_expire_statement(std::vector<Token>& toks);

    template <typename Fn>
    static std::optional<std::unique_ptr<Statement>> dispatch_key_type(std::vector<Token>& toks, Fn&& fn) {
        if (toks.size() < 2) {
            return std::nullopt;
        }

        return dispatch_value_type(toks[1], std::forward<Fn>(fn));
    }

    template <typename Fn>
    static std::optional<std::unique_ptr<Statement>> dispatch_value_type(const Token& tok, Fn&& fn) {
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
                return std::nullopt;
        }
    }
};



#endif //PARSER_H
