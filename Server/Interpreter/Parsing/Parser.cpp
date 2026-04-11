//
// Created by Mark on 4/5/26.
//

#include "Interpreter/Parsing/Parser.h"

#include <limits>
#include <sstream>

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

std::unique_ptr<GetStatement> Parser::try_parse_get(std::vector<Token>& toks) {
    if (toks.size() != kUnaryStatementTokenCount ||
        toks[kCommandTokenIndex].get_type() != TokenType::GET ||
        !toks[kFirstArgumentTokenIndex].has_value()) {
        return nullptr;
    }

    auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
    if (!key.has_value()) {
        return nullptr;
    }

    return std::make_unique<GetStatement>(key.value());
}

template <typename V>
std::unique_ptr<SetStatement<V>> Parser::try_parse_set(std::vector<Token>& toks) {
    if (toks.size() != kBinaryStatementTokenCount ||
        toks[kCommandTokenIndex].get_type() != TokenType::SET ||
        !toks[kFirstArgumentTokenIndex].has_value() ||
        !toks[kSecondArgumentTokenIndex].has_value()) {
        return nullptr;
    }

    auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
    auto val = toks[kSecondArgumentTokenIndex].template get_prim<V>();
    if (!key.has_value() || !val.has_value()) {
        return nullptr;
    }

    return std::make_unique<SetStatement<V>>(key.value(), val.value());
}

std::unique_ptr<DeleteStatement> Parser::try_parse_del(std::vector<Token>& toks) {
    if (toks.size() != kUnaryStatementTokenCount ||
        toks[kCommandTokenIndex].get_type() != TokenType::DEL ||
        !toks[kFirstArgumentTokenIndex].has_value()) {
        return nullptr;
    }

    auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
    if (!key.has_value()) {
        return nullptr;
    }

    return std::make_unique<DeleteStatement>(key.value());
}

std::unique_ptr<ExistsStatement> Parser::try_parse_exists(std::vector<Token>& toks) {
    if (toks.size() != kUnaryStatementTokenCount ||
        toks[kCommandTokenIndex].get_type() != TokenType::EXISTS ||
        !toks[kFirstArgumentTokenIndex].has_value()) {
        return nullptr;
    }

    auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
    if (!key.has_value()) {
        return nullptr;
    }

    return std::make_unique<ExistsStatement>(key.value());
}

std::unique_ptr<ExpireStatement> Parser::try_parse_expire(std::vector<Token>& toks) {
    if (toks.size() != kBinaryStatementTokenCount ||
        toks[kCommandTokenIndex].get_type() != TokenType::EXPIRE ||
        toks[kSecondArgumentTokenIndex].get_type() != TokenType::INT ||
        !toks[kFirstArgumentTokenIndex].has_value()) {
        return nullptr;
    }

    auto key = key_from_token(toks[kFirstArgumentTokenIndex]);
    auto time = toks[kSecondArgumentTokenIndex].template get_prim<int>();
    if (!key.has_value() || !time.has_value()) {
        return nullptr;
    }

    return std::make_unique<ExpireStatement>(key.value(), time.value());
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

    switch (toks[2].get_type()) {
        case TokenType::INT: {
            auto parsed = Parser::try_parse_set<int>(toks);
            if (parsed == nullptr) {
                return nullptr;
            }

            std::unique_ptr<Statement> statement = std::move(parsed);
            return statement;
        }
        case TokenType::DOUBLE: {
            auto parsed = Parser::try_parse_set<double>(toks);
            if (parsed == nullptr) {
                return nullptr;
            }

            std::unique_ptr<Statement> statement = std::move(parsed);
            return statement;
        }
        case TokenType::CHAR: {
            auto parsed = Parser::try_parse_set<char>(toks);
            if (parsed == nullptr) {
                return nullptr;
            }

            std::unique_ptr<Statement> statement = std::move(parsed);
            return statement;
        }
        case TokenType::STRING: {
            auto parsed = Parser::try_parse_set<std::string>(toks);
            if (parsed == nullptr) {
                return nullptr;
            }

            std::unique_ptr<Statement> statement = std::move(parsed);
            return statement;
        }
        default:
            return nullptr;
    }
}

std::unique_ptr<Statement> Parser::parse_expire_statement(std::vector<Token>& toks) {
    auto parsed = Parser::try_parse_expire(toks);
    if (parsed == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Statement> statement = std::move(parsed);
    return statement;
}

std::optional<Key> Parser::key_from_token(const Token& tok) {
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

template std::unique_ptr<SetStatement<int>> Parser::try_parse_set<int>(std::vector<Token>& toks);
template std::unique_ptr<SetStatement<double>> Parser::try_parse_set<double>(std::vector<Token>& toks);
template std::unique_ptr<SetStatement<char>> Parser::try_parse_set<char>(std::vector<Token>& toks);
template std::unique_ptr<SetStatement<std::string>> Parser::try_parse_set<std::string>(std::vector<Token>& toks);
