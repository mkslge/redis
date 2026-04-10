//
// Created by Mark on 4/5/26.
//

#ifndef PRIMTOKEN_H
#define PRIMTOKEN_H
#include "Token.h"

template <typename T>
class PrimToken : public Token {
public:
    PrimToken(TokenType type, const T& val) : Token(type, val) {
     };
    T get_val() const { return this->template get_prim<T>().value(); }

    bool is_prim() const override {
        return true;
    }

    bool operator==(const Token& other) const override {
        return Token::operator==(other);
    }

    bool operator==(const PrimToken& other) const {
        return Token::operator==(other);
    }
};



#endif //PRIMTOKEN_H

