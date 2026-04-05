//
// Created by Mark on 4/5/26.
//

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "TokenType.h"
#include <vector>
#include <string_view>
#include <string>
#include "../Utility/util.cpp"
class Tokenizer {
public:
    Tokenizer();
    static std::optional<std::vector<TokenType>> tokenize(std::string& input) {
        //at the beginning we want set all input to lowercase to normalize it.
        input = util::to_lower(input);
        int idx = 0;
        std::vector<TokenType> token_list{};

        while (idx < input.size()) {
            auto token = tokenize_once(input, idx); //idx incremented here
            if (token == std::nullopt) {
                return std::nullopt;
            }
            token_list.push_back(token.value());

        }

        return token_list;
    }

private:
    static std::optional<TokenType> tokenize_once(std::string& input, int& idx_out) {

        if (input.substr(idx_out, 3) == "get") {
            idx_out += 3;
            return TokenType::GET;
        } else if (input.substr(idx_out, 3) == "set") {
            idx_out += 3;
            return TokenType::SET;
        } else if (input.substr(idx_out, 3) == "del") {
            idx_out += 3;
            return TokenType::DEL;
        } else if (input.substr(idx_out, 3) == "key") {
            idx_out += 3;
            return TokenType::key;
        } else if (input.substr(idx_out, 5) == "value") {
            idx_out += 5;
            return TokenType::value;
        }
        else if (input.substr(idx_out, 6) == "exists") {
            idx_out += 6;
            return TokenType::EXISTS;
        } else if (input.substr(idx_out, 6) == "expire") {
            idx_out += 6;
            return TokenType::EXPIRE;
        } else if (input.substr(idx_out, 7) == "seconds") {
            idx_out += 7;
            return TokenType::seconds;
        }

        return std::nullopt;
    }

};



#endif //TOKENIZER_H
