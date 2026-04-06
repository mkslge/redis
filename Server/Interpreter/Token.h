//
// Created by Mark on 4/5/26.
//

#ifndef TOKEN_H
#define TOKEN_H

#include "TokenType.h"

class Token {
private:
    TokenType type_;
public:
    explicit Token(TokenType type) : type_(type) {}
    TokenType get_type() const { return type_; }
    virtual bool operator==(const Token& other) const {
        return this->type_ == other.type_ ;
    }
    virtual ~Token() = default;
};

#endif //TOKEN_H
