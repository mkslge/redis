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

    bool is_prim() const override {
        return true;
    }

    bool operator==(const Token other) const {
        if (!other.is_prim()) {
            return false;
        }
        auto other_cpy = static_cast<PrimToken>(other);
        return this->get_type() == other_cpy.get_type() && val_ == other_cpy.get_val();
    }

    bool operator==(const PrimToken& other) const {
        return this->get_type() == other.get_type() && val_ == other.get_val();
    }
};



#endif //PRIMTOKEN_H


