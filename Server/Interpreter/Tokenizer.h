//
// Created by Mark on 4/5/26.
//

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "Token.h"
#include "PrimToken.h"
#include "../Utility/util.h"
#include <vector>
#include <string_view>
#include <string>

#include <iostream>
#include <charconv>
class Tokenizer {
public:
    Tokenizer();
    static std::optional<std::vector<Token>> tokenize(std::string& input) {
        //at the beginning we want set all input to lowercase to normalize it.
        input = util::to_lower(input);
        int idx = 0;
        std::vector<Token> token_list{};

        while (idx < input.size()) {
            if (input[idx] == ' ') {
                idx++;
                continue;
            }
            auto token = tokenize_once(input, idx); //idx incremented here
            if (token == std::nullopt) {
                return std::nullopt;
            }
            token_list.push_back(token.value());

        }

        return token_list.size() > 0 ?
    std::optional<std::vector<Token>>{token_list} : std::nullopt;
    }

private:
    static std::optional<Token> tokenize_once(std::string& input, int& idx_out) {

        if (input.substr(idx_out, 3) == "get") {
            idx_out += 3;
            return Token(TokenType::GET);
        } else if (input.substr(idx_out, 3) == "set") {
            idx_out += 3;
            return Token(TokenType::SET);
        } else if (input.substr(idx_out, 3) == "del") {
            idx_out += 3;
            return Token(TokenType::DEL);
        } else if (input.substr(idx_out, 6) == "exists") {
            idx_out += 6;
            return Token(TokenType::EXISTS);
        } else if (input.substr(idx_out, 6) == "expire") {
            idx_out += 6;
            return Token(TokenType::EXPIRE);
        }
        std::optional<int> opt_int = is_int(input, idx_out);
        if (opt_int.has_value()) {
            return PrimToken<int>( TokenType::INT, opt_int.value());
        }

        return std::nullopt;
    }


    static std::optional<int> is_int(const std::string& input, int& idx_out) {
        int value;
        const char* start = input.data() + idx_out;
        const char* end   = input.data() + input.size();

        auto [ptr, ec] = std::from_chars(start, end, value);

        if (ec != std::errc()) {
            return std::nullopt;
        }

        idx_out = static_cast<int>(ptr - input.data());
        return value;
    }

};



#endif //TOKENIZER_H
