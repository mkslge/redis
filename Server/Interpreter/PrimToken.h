//
// Created by Mark on 4/5/26.
//

#ifndef PRIMTOKEN_H
#define PRIMTOKEN_H
#include "Token.h"

template <typename T>
class PrimToken : public Token {
private:
     T val_;
public:
    PrimToken(TokenType type, const T& val) : Token(type),  val_(val) {
     };
    const T& get_val() const {return val_;}
};

#endif //PRIMTOKEN_H


