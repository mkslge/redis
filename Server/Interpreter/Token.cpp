//
// Created by Mark on 4/10/26.
//

#include "Token.h"

Token::Token(TokenType type) : type_(type) {}

TokenType Token::get_type() const {
    return type_;
}

bool Token::is_prim() const {
    return !std::holds_alternative<std::monostate>(value_);
}

bool Token::operator==(const Token& other) const {
    return this->type_ == other.type_ && value_ == other.value_;
}

Token::~Token() = default;
