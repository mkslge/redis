//
// Created by Mark on 4/5/26.
//

#ifndef TOKEN_H
#define TOKEN_H

#include "TokenType.h"
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

class Token {
private:
    TokenType type_;
    std::variant<std::monostate, std::int64_t, double, char, std::string> value_{};
public:
    explicit Token(TokenType type);
    template <typename T>
    Token(TokenType type, const T& value) : type_(type), value_(value) {}
    template <typename T>
    Token(TokenType type, const T& value, std::string source_text)
        : type_(type), value_(value), source_text_(std::move(source_text)) {}

    TokenType get_type() const;

    bool has_value() const;
    const std::optional<std::string>& source_text() const;

    template <typename T>
    std::optional<T> get_prim() const {
        if (const auto* value = std::get_if<T>(&value_)) {
            return *value;
        }

        return std::nullopt;
    }

    bool operator==(const Token& other) const;
    ~Token();

private:
    std::optional<std::string> source_text_;
};

#endif //TOKEN_H
