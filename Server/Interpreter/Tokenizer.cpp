//
// Created by Mark on 4/5/26.
//

#include "Tokenizer.h"

Tokenizer::Tokenizer() = default;

std::optional<std::vector<Token>> Tokenizer::tokenize(std::string& input) {
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

std::optional<Token> Tokenizer::tokenize_once(std::string& input, int& idx_out) {
    std::string substr_three = input.substr(idx_out, 3);
    std::string substr_six = input.substr(idx_out, 6);
    if (util::to_lowercase(substr_three) == "get") {
        idx_out += 3;
        return Token(TokenType::GET);
    } else if (util::to_lowercase(substr_three) == "set") {
        idx_out += 3;
        return Token(TokenType::SET);
    } else if (util::to_lowercase(substr_three) == "del") {
        idx_out += 3;
        return Token(TokenType::DEL);
    } else if (util::to_lowercase(substr_six) == "exists") {
        idx_out += 6;
        return Token(TokenType::EXISTS);
    } else if (util::to_lowercase(substr_six) == "expire") {
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

std::optional<int> Tokenizer::is_int(const std::string& input, int& idx_out) {
    int value;
    const char* start = input.data() + idx_out;
    const char* end   = input.data() + input.size();

    auto [ptr, ec] = std::from_chars(start, end, value);

    if (ec != std::errc()) {
        return std::nullopt;
    }

    if (ptr < end && (*ptr == '.' || *ptr == 'e' || *ptr == 'E')) {
        return std::nullopt;
    }

    idx_out = static_cast<int>(ptr - input.data());
    return value;
}

std::optional<double> Tokenizer::is_double(const std::string& input, int& idx_out) {
    const char* start = input.c_str() + idx_out;
    char* end = nullptr;

    errno = 0;
    double value = std::strtod(start, &end);

    if (start == end || errno == ERANGE) {
        return std::nullopt;
    }

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

std::optional<char> Tokenizer::is_char(const std::string& input, int& idx_out) {
    if (idx_out + 2 >= static_cast<int>(input.size()) || input[idx_out] != '\'' || input[idx_out + 2] != '\'') {
        return std::nullopt;
    }

    const char value = input[idx_out + 1];
    idx_out += 3;
    return value;
}

std::optional<std::string> Tokenizer::is_str(const std::string& input, int& idx_out) {
    if (input[idx_out] != '\"') {
        return std::nullopt;
    }

    idx_out++;
    std::string builder;
    while (idx_out < static_cast<int>(input.size())) {
        if (input[idx_out] == '\"') {
            idx_out++;
            return builder;
        }
        builder += input[idx_out++];
    }

    return std::nullopt;
}
