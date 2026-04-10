//
// Created by Mark on 4/5/26.
//

#ifndef TOKEN_H
#define TOKEN_H

#include "TokenType.h"
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

    virtual bool is_prim() const;

    template <typename T>
    std::optional<T> get_prim() const {
        if (const auto* value = std::get_if<T>(&value_)) {
            return *value;
        }

        return std::nullopt;
    }

    virtual bool operator==(const Token& other) const;
    virtual ~Token();
};

/*std::iostream& operator<<(std::ostream& os, const Token tok) {
    os << "Token(type=" << token_type_str(tok.get_type()) << ")";
    return dynamic_cast<std::iostream &>(os);
}*/

#endif //TOKEN_H
