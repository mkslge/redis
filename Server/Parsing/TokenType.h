//
// Created by Mark on 4/5/26.
//

#ifndef TOKENTYPE_H
#define TOKENTYPE_H

#include <string>

enum class TokenType {
   SET, GET, DEL, EXISTS, EXPIRE, TTL, PTTL, PERSIST,
   INT, DOUBLE, CHAR, STRING
};

inline std::string token_type_str(const TokenType& type) {
   switch (type) {
      case TokenType::SET:
         return "SET";
      case TokenType::GET:
         return "GET";
      case TokenType::DEL:
         return "DEL";
      case TokenType::EXISTS:
         return "EXISTS";
      case TokenType::EXPIRE:
         return "EXPIRE";
      case TokenType::TTL:
         return "TTL";
      case TokenType::PTTL:
         return "PTTL";
      case TokenType::PERSIST:
         return "PERSIST";
      case TokenType::INT:
         return "INT";
      case TokenType::DOUBLE:
         return "DOUBLE";

      case TokenType::CHAR:
         return "CHAR";
      case TokenType::STRING:
         return "STRING";
   }
   return "err";

}

#endif //TOKENTYPE_H
