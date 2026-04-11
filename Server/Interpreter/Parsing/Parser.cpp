//
// Created by Mark on 4/5/26.
//

#include "Interpreter/Parsing/Parser.h"

std::unique_ptr<Statement> Parser::parse(std::vector<Token>& toks) {
    if (toks.empty()) {
        return nullptr;
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
            return nullptr;
    }
}

std::unique_ptr<Statement> Parser::parse_get_statement(std::vector<Token>& toks) {
    auto parsed = Parser::try_parse_get(toks);
    if (parsed == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Statement> statement = std::move(parsed);
    return statement;
}

std::unique_ptr<Statement> Parser::parse_del_statement(std::vector<Token>& toks) {
    auto parsed = Parser::try_parse_del(toks);
    if (parsed == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Statement> statement = std::move(parsed);
    return statement;
}

std::unique_ptr<Statement> Parser::parse_exists_statement(std::vector<Token>& toks) {
    auto parsed = Parser::try_parse_exists(toks);
    if (parsed == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Statement> statement = std::move(parsed);
    return statement;
}

std::unique_ptr<Statement> Parser::parse_set_statement(std::vector<Token>& toks) {
    if (toks.size() != 3 || !toks[1].has_value() || !toks[2].has_value()) {
        return nullptr;
    }

    return dispatch_value_type(
        toks[2],
        [&]<typename V>() -> std::unique_ptr<Statement> {
            auto parsed = Parser::template try_parse_set<V>(toks);
            if (parsed == nullptr) {
                return nullptr;
            }

            std::unique_ptr<Statement> statement = std::move(parsed);
            return statement;
        }
    );
}

std::unique_ptr<Statement> Parser::parse_expire_statement(std::vector<Token>& toks) {
    auto parsed = Parser::try_parse_expire(toks);
    if (parsed == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Statement> statement = std::move(parsed);
    return statement;
}
