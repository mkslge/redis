//
// Created by Mark on 4/5/26.
//

#ifndef TOKEN_H
#define TOKEN_H

#include "Parsing/TokenType.h"
#include <optional>
#include <string>
#include <variant>

class Token {
private:
    TokenType type_;
    std::variant<std::monostate, int, double, char, std::string> value_{};
public:
    explicit Token(TokenType type);
    template <typename T>
    Token(TokenType type, const T& value) : type_(type), value_(value) {}

    TokenType get_type() const;

    bool has_value() const;

    template <typename T>
    std::optional<T> get_prim() const {
        if (const auto* value = std::get_if<T>(&value_)) {
            return *value;
        }

        return std::nullopt;
    }

    bool operator==(const Token& other) const;
    ~Token();
};

#endif //TOKEN_H
