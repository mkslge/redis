//
// Created by Mark on 4/5/26.
//

#ifndef TOKENTYPE_H
#define TOKENTYPE_H

enum class TokenType {
   SET, GET, DEL, EXISTS, EXPIRE,
   key, value, seconds
};

#endif //TOKENTYPE_H
