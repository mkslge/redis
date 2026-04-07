//
// Created by Mark on 4/5/26.
//

#ifndef TOKEN_H
#define TOKEN_H

#include "TokenType.h"
#include <iostream>
class Token {
private:
    TokenType type_;
public:
    explicit Token(TokenType type) : type_(type) {}
    TokenType get_type() const { return type_; }

    virtual bool is_prim() const {
        return false;
    }

    virtual bool operator==(const Token& other) const {
        return this->type_ == other.type_ ;
    }
    virtual ~Token() = default;
};

/*std::iostream& operator<<(std::ostream& os, const Token tok) {
    os << "Token(type=" << token_type_str(tok.get_type()) << ")";
    return dynamic_cast<std::iostream &>(os);
}*/

#endif //TOKEN_H
