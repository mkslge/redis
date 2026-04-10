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

    static std::optional<std::vector<Token>> tokenize(std::string& input);

private:
    static std::optional<Token> tokenize_once(std::string& input, int& idx_out);
    static std::optional<int> is_int(const std::string& input, int& idx_out);
    static std::optional<double> is_double(const std::string& input, int& idx_out);
    static std::optional<char> is_char(const std::string& input, int& idx_out);
    static std::optional<std::string> is_str(const std::string& input, int &idx_out);
};

#endif // TOKENIZER_H
