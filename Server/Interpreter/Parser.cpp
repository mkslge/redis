//
// Created by Mark on 4/5/26.
//

#include "Parser.h"

std::optional<std::unique_ptr<Statement>> Parser::parse(std::vector<Token>& toks) {
    if (toks.empty()) {
        return std::nullopt;
    }

    switch (toks[0].get_type()) {
        case TokenType::GET:
            return parse_get_statement(toks);
        case TokenType::SET:
            return parse_set_statement(toks);
        case TokenType::DEL:
            return parse_del_statement(toks);
        case TokenType::EXISTS:
            return parse_exists_statement(toks);
        case TokenType::EXPIRE:
            return parse_expire_statement(toks);
        default:
            return std::nullopt;
    }
}

std::optional<std::unique_ptr<Statement>> Parser::parse_get_statement(std::vector<Token>& toks) {
    return dispatch_key_type(
        toks,
        [&]<typename K>() -> std::optional<std::unique_ptr<Statement>> {
            auto parsed = Parser::template try_parse_get<K>(toks);
            if (!parsed.has_value()) {
                return std::nullopt;
            }

            std::unique_ptr<Statement> statement = std::move(parsed.value());
            return statement;
        }
    );
}

std::optional<std::unique_ptr<Statement>> Parser::parse_del_statement(std::vector<Token>& toks) {
    return dispatch_key_type(
        toks,
        [&]<typename K>() -> std::optional<std::unique_ptr<Statement>> {
            auto parsed = Parser::template try_parse_del<K>(toks);
            if (!parsed.has_value()) {
                return std::nullopt;
            }

            std::unique_ptr<Statement> statement = std::move(parsed.value());
            return statement;
        }
    );
}

std::optional<std::unique_ptr<Statement>> Parser::parse_exists_statement(std::vector<Token>& toks) {
    return dispatch_key_type(
        toks,
        [&]<typename K>() -> std::optional<std::unique_ptr<Statement>> {
            auto parsed = Parser::template try_parse_exists<K>(toks);
            if (!parsed.has_value()) {
                return std::nullopt;
            }

            std::unique_ptr<Statement> statement = std::move(parsed.value());
            return statement;
        }
    );
}

std::optional<std::unique_ptr<Statement>> Parser::parse_set_statement(std::vector<Token>& toks) {
    if (toks.size() != 3 || !toks[1].is_prim() || !toks[2].is_prim()) {
        return std::nullopt;
    }

    return dispatch_key_type(
        toks,
        [&]<typename K>() -> std::optional<std::unique_ptr<Statement>> {
            return dispatch_value_type(
                toks[2],
                [&]<typename V>() -> std::optional<std::unique_ptr<Statement>> {
                    auto parsed = Parser::template try_parse_set<K, V>(toks);
                    if (!parsed.has_value()) {
                        return std::nullopt;
                    }

                    std::unique_ptr<Statement> statement = std::move(parsed.value());
                    return statement;
                }
            );
        }
    );
}

std::optional<std::unique_ptr<Statement>> Parser::parse_expire_statement(std::vector<Token>& toks) {
    return dispatch_key_type(
        toks,
        [&]<typename K>() -> std::optional<std::unique_ptr<Statement>> {
            auto parsed = Parser::template try_parse_expire<K>(toks);
            if (!parsed.has_value()) {
                return std::nullopt;
            }

            std::unique_ptr<Statement> statement = std::move(parsed.value());
            return statement;
        }
    );
}
