//
// Created by Mark on 4/5/26.
//

#include "Interpreter/Parsing/Tokenizer.h"

#include <string_view>

namespace {
constexpr int kSingleCharStep = 1;
constexpr int kCharTokenLength = 3;

constexpr char kWhitespaceChar = ' ';
constexpr char kStringDelimiter = '"';
constexpr char kCharDelimiter = '\'';
constexpr char kDecimalPoint = '.';
constexpr char kExponentLower = 'e';
constexpr char kExponentUpper = 'E';

constexpr std::string_view kGetKeyword = "get";
constexpr std::string_view kSetKeyword = "set";
constexpr std::string_view kDelKeyword = "del";
constexpr std::string_view kExistsKeyword = "exists";
constexpr std::string_view kExpireKeyword = "expire";

struct KeywordSpec {
    std::string_view keyword;
    TokenType type;
};

constexpr KeywordSpec kThreeCharKeywords[] = {
    {kGetKeyword, TokenType::GET},
    {kSetKeyword, TokenType::SET},
    {kDelKeyword, TokenType::DEL},
};

constexpr KeywordSpec kSixCharKeywords[] = {
    {kExistsKeyword, TokenType::EXISTS},
    {kExpireKeyword, TokenType::EXPIRE},
};

bool is_token_boundary(const std::string& input, int idx) {
    return idx >= static_cast<int>(input.size()) || input[idx] == kWhitespaceChar;
}

bool matches_keyword_case_insensitive(
    const std::string& input,
    int idx,
    std::string_view keyword
) {
    if (idx + static_cast<int>(keyword.size()) > static_cast<int>(input.size())) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(keyword.size()); ++i) {
        if (std::tolower(static_cast<unsigned char>(input[idx + i])) != keyword[i]) {
            return false;
        }
    }

    return true;
}

std::optional<Token> try_keyword(
    const std::string& input,
    int& idx_out,
    const KeywordSpec* specs,
    std::size_t spec_count
) {
    for (std::size_t i = 0; i < spec_count; ++i) {
        const KeywordSpec& spec = specs[i];
        const int end_idx = idx_out + static_cast<int>(spec.keyword.size());

        if (!matches_keyword_case_insensitive(input, idx_out, spec.keyword)) {
            continue;
        }

        if (!is_token_boundary(input, end_idx)) {
            continue;
        }

        idx_out = end_idx;
        return Token(spec.type);
    }

    return std::nullopt;
}
} // namespace


bool Tokenizer::is_token_boundary(const std::string& input, int idx) {
    return ::is_token_boundary(input, idx);
}

std::optional<std::vector<Token>> Tokenizer::tokenize(const std::string& input) {
    int idx = 0;
    std::vector<Token> token_list{};

    while (idx < static_cast<int>(input.size())) {
        if (input[idx] == kWhitespaceChar) {
            idx += kSingleCharStep;
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

std::optional<Token> Tokenizer::tokenize_once(const std::string& input, int& idx_out) {
    if (auto token = try_keyword(input, idx_out, kThreeCharKeywords, std::size(kThreeCharKeywords))) {
        return token;
    }

    if (auto token = try_keyword(input, idx_out, kSixCharKeywords, std::size(kSixCharKeywords))) {
        return token;
    }

    {
        int temp_idx = idx_out;
        std::optional<int> opt_int = is_int(input, temp_idx);
        if (opt_int.has_value()) {
            idx_out = temp_idx;
            return Token(TokenType::INT, opt_int.value());
        }
    }

    {
        int temp_idx = idx_out;
        std::optional<double> opt_dbl = is_double(input, temp_idx);
        if (opt_dbl.has_value()) {
            idx_out = temp_idx;
            return Token(TokenType::DOUBLE, opt_dbl.value());
        }
    }

    {
        int temp_idx = idx_out;
        std::optional<char> opt_ch = is_char(input, temp_idx);
        if (opt_ch.has_value()) {
            idx_out = temp_idx;
            return Token(TokenType::CHAR, opt_ch.value());
        }
    }

    {
        int temp_idx = idx_out;
        std::optional<std::string> opt_str = is_str(input, temp_idx);
        if (opt_str.has_value()) {
            idx_out = temp_idx;
            return Token(TokenType::STRING, opt_str.value());
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

    if (ptr < end && (*ptr == kDecimalPoint || *ptr == kExponentLower || *ptr == kExponentUpper)) {
        return std::nullopt;
    }

    const int end_idx = static_cast<int>(ptr - input.data());
    if (!is_token_boundary(input, end_idx)) {
        return std::nullopt;
    }

    idx_out = end_idx;
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
        if (*p == kDecimalPoint || *p == kExponentLower || *p == kExponentUpper) {
            saw_double_syntax = true;
            break;
        }
    }

    if (!saw_double_syntax) {
        return std::nullopt;
    }

    const int end_idx = static_cast<int>(end - input.c_str());
    if (!is_token_boundary(input, end_idx)) {
        return std::nullopt;
    }

    idx_out = end_idx;
    return value;
}

std::optional<char> Tokenizer::is_char(const std::string& input, int& idx_out) {
    if (idx_out + (kCharTokenLength - 1) >= static_cast<int>(input.size()) ||
        input[idx_out] != kCharDelimiter ||
        input[idx_out + (kCharTokenLength - 1)] != kCharDelimiter) {
        return std::nullopt;
    }

    const char value = input[idx_out + kSingleCharStep];
    const int end_idx = idx_out + kCharTokenLength;
    if (!is_token_boundary(input, end_idx)) {
        return std::nullopt;
    }

    idx_out = end_idx;
    return value;
}

std::optional<std::string> Tokenizer::is_str(const std::string& input, int& idx_out) {
    if (input[idx_out] != kStringDelimiter) {
        return std::nullopt;
    }

    idx_out += kSingleCharStep;
    std::string builder;
    while (idx_out < static_cast<int>(input.size())) {
        if (input[idx_out] == kStringDelimiter) {
            const int end_idx = idx_out + kSingleCharStep;
            if (!is_token_boundary(input, end_idx)) {
                return std::nullopt;
            }

            idx_out = end_idx;
            return builder;
        }
        builder += input[idx_out++];
    }

    return std::nullopt;
}
