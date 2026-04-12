#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "Parsing/Token.h"
#include "Utility/util.h"
#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

class Tokenizer {
public:
    static std::optional<std::vector<Token>> tokenize(const std::string& input);

private:
    static std::optional<Token> tokenize_once(const std::string& input, int& idx_out);
    static std::optional<int> is_int(const std::string& input, int& idx_out);
    static std::optional<double> is_double(const std::string& input, int& idx_out);
    static std::optional<char> is_char(const std::string& input, int& idx_out);
    static std::optional<std::string> is_str(const std::string& input, int &idx_out);
    static bool is_token_boundary(const std::string& input, int idx);
};

#endif // TOKENIZER_H
