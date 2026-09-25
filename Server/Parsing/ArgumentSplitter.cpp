#include "ArgumentSplitter.h"

#include <cctype>
#include <cstddef>
#include <utility>

namespace {
bool is_separator(const char byte) {
    return std::isspace(static_cast<unsigned char>(byte)) != 0;
}

int hex_digit_value(const char byte) {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}

char unescape(const char byte) {
    switch (byte) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'a': return '\a';
        default: return byte;
    }
}

// A closing quote must end the argument, as in redis-cli.
bool closes_argument(const std::string_view line, const std::size_t quote_index) {
    return quote_index + 1 == line.size() || is_separator(line[quote_index + 1]);
}
}

std::optional<CommandArguments> ArgumentSplitter::split(const std::string_view line) {
    CommandArguments arguments;
    std::size_t index = 0;

    while (true) {
        while (index < line.size() && is_separator(line[index])) ++index;
        if (index == line.size()) return arguments;

        Bytes current;
        bool in_double_quotes = false;
        bool in_single_quotes = false;
        bool done = false;
        while (!done) {
            if (in_double_quotes) {
                if (index == line.size()) return std::nullopt;
                const char byte = line[index];
                if (byte == '\\' && index + 3 < line.size() && line[index + 1] == 'x' &&
                    hex_digit_value(line[index + 2]) >= 0 && hex_digit_value(line[index + 3]) >= 0) {
                    current.push_back(static_cast<char>(
                        hex_digit_value(line[index + 2]) * 16 + hex_digit_value(line[index + 3])));
                    index += 3;
                } else if (byte == '\\' && index + 1 < line.size()) {
                    current.push_back(unescape(line[++index]));
                } else if (byte == '"') {
                    if (!closes_argument(line, index)) return std::nullopt;
                    done = true;
                } else {
                    current.push_back(byte);
                }
            } else if (in_single_quotes) {
                if (index == line.size()) return std::nullopt;
                const char byte = line[index];
                if (byte == '\\' && index + 1 < line.size() && line[index + 1] == '\'') {
                    current.push_back('\'');
                    ++index;
                } else if (byte == '\'') {
                    if (!closes_argument(line, index)) return std::nullopt;
                    done = true;
                } else {
                    current.push_back(byte);
                }
            } else {
                if (index == line.size() || is_separator(line[index])) break;
                const char byte = line[index];
                if (byte == '"') in_double_quotes = true;
                else if (byte == '\'') in_single_quotes = true;
                else current.push_back(byte);
            }
            ++index;
        }
        arguments.push_back(std::move(current));
    }
}
