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


class Parser {
public:
    static std::optional<std::unique_ptr<Statement>> parse(std::vector<Token>& toks) {


        return nullptr;
    }

    template <typename K>
    static std::optional<std::unique_ptr<GetStatement<K>>> try_parse_get(std::vector<Token>& toks) {
        if (toks.size() != 2 ||
            toks[0].get_type() != TokenType::GET
            || toks[1].is_prim()) {
            return std::nullopt;
        }


        auto key = static_cast<PrimToken<K>>(toks[1]).get_val();

        return std::move(std::make_unique<GetStatement<K>>(GetStatement<K>(key)));
    }

    template <typename K, typename V>
    static std::optional<std::unique_ptr<SetStatement<K, V>>> try_parse_set(std::vector<Token>& toks) {
        if (toks.size() != 3 ||
            toks[0].get_type() != TokenType::SET
            || !toks[1].is_prim()
            || !toks[2].is_prim()) {
            return std::nullopt;
        }


        auto key = static_cast<PrimToken<K>>(toks[1]).get_val();
        auto val = static_cast<PrimToken<V>>(toks[2]).get_val();

        return std::move(std::make_unique<SetStatement<K, V>>(SetStatement<K, V>(key, val)));
    }

    template <typename K>
    static std::optional<std::unique_ptr<DeleteStatement<K>>> try_parse_del(std::vector<Token>& toks) {
        if (toks.size() != 2 ||
            toks[0].get_type() != TokenType::DEL
            || toks[1].is_prim()) {
            return std::nullopt;
            }


        auto key = static_cast<PrimToken<K>>(toks[1]).get_val();

        return std::move(std::make_unique<DeleteStatement<K>>(DeleteStatement<K>(key)));
    }

    template <typename K>
    static std::optional<std::unique_ptr<ExistsStatement<K>>> try_parse_exists(std::vector<Token>& toks) {
        if (toks.size() != 2 ||
            toks[0].get_type() != TokenType::EXISTS
            || toks[1].is_prim()) {
            return std::nullopt;
            }


        auto key = static_cast<PrimToken<K>>(toks[1]).get_val();

        return std::move(std::make_unique<ExistsStatement<K>>(ExistsStatement<K>(key)));
    }

    template <typename K>
    static std::optional<std::unique_ptr<SetStatement<K, V>>> try_parse_expire(std::vector<Token>& toks) {
        if (toks.size() != 3 ||
            toks[0].get_type() != TokenType::SET
            || toks[2].get_type() != TokenType::INT
            || !toks[1].is_prim()
            ) {
            return std::nullopt;
            }


        auto key = static_cast<PrimToken<K>>(toks[1]).get_val();
        auto time = static_cast<PrimToken<int>>(toks[2]).get_val();


        return std::move(std::make_unique<ExpireStatement<K>>(ExpireStatement<K>(key, time)));
    }

};



#endif //PARSER_H
