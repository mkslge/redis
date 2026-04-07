#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "Token.h"
#include "PrimToken.h"
#include "../Utility/util.h"
#include <vector>
#include <string_view>
#include <string>
#include <optional>
#include <charconv>
#include <cstdlib>
#include <cerrno>
#include <cctype>

class Tokenizer {
public:
    Tokenizer();

    static std::optional<std::vector<Token>> tokenize(std::string& input) {
        input = util::to_lower(input);
        int idx = 0;
        std::vector<Token> token_list{};

        while (idx < static_cast<int>(input.size())) {
            if (input[idx] == ' ') {
                idx++;
                continue;
            }

            auto token = tokenize_once(input, idx);
            if (token == std::nullopt) {
                return std::nullopt;
            }

            token_list.push_back(token.value());
        }

        return token_list.empty()
            ? std::nullopt
            : std::optional<std::vector<Token>>{token_list};
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

        {
            int temp_idx = idx_out;
            std::optional<int> opt_int = is_int(input, temp_idx);
            if (opt_int.has_value()) {
                idx_out = temp_idx;
                return PrimToken<int>(TokenType::INT, opt_int.value());
            }
        }

        {
            int temp_idx = idx_out;
            std::optional<double> opt_dbl = is_double(input, temp_idx);
            if (opt_dbl.has_value()) {
                idx_out = temp_idx;
                return PrimToken<double>(TokenType::DOUBLE, opt_dbl.value());
            }
        }

        {
            int temp_idx = idx_out;
            std::optional<char> opt_ch = is_char(input, temp_idx);
            if (opt_ch.has_value()) {
                idx_out = temp_idx;
                return PrimToken<char>(TokenType::CHAR, opt_ch.value());
            }
        }

        {
            int temp_idx = idx_out;
            std::optional<std::string> opt_str = is_str(input, temp_idx);
            if (opt_str.has_value()) {
                idx_out = temp_idx;
                return PrimToken<std::string>(TokenType::STRING, opt_str.value());
            }
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

        //reject if this integer is actually the prefix of a double/exponent form.
        if (ptr < end && (*ptr == '.' || *ptr == 'e' || *ptr == 'E')) {
            return std::nullopt;
        }

        idx_out = static_cast<int>(ptr - input.data());
        return value;
    }

    static std::optional<double> is_double(const std::string& input, int& idx_out) {
        const char* start = input.c_str() + idx_out;
        char* end = nullptr;

        errno = 0;
        double value = std::strtod(start, &end);

        if (start == end || errno == ERANGE) {
            return std::nullopt;
        }

        //make sure we actually parsed a floating-point form,
        //not just something that should be treated as an int.
        bool saw_double_syntax = false;
        for (const char* p = start; p < end; ++p) {
            if (*p == '.' || *p == 'e' || *p == 'E') {
                saw_double_syntax = true;
                break;
            }
        }

        if (!saw_double_syntax) {
            return std::nullopt;
        }

        idx_out = static_cast<int>(end - input.c_str());
        return value;
    }

    static std::optional<char> is_char(const std::string& input, int& idx_out) {
        if (input[idx_out] != '\'' || idx_out + 2 >= input.size() || input[idx_out] != '\'') {
            return std::nullopt;
        }
        idx_out += 3;
        return idx_out - 2;
    }

    static std::optional<std::string> is_str(const std::string& input, int &idx_out) {
        if (input[idx_out] != '\"') {
            return std::nullopt;
        }
        idx_out++;
        std::string builder = "";
        while (idx_out < input.size()) {
            if (input[idx_out] == '\"') {
                idx_out++;
                return builder;
            }
            builder += input[idx_out++];
        }


        return std::nullopt;
    }
};

#endif // TOKENIZER_H