#ifndef PARSER_H
#define PARSER_H

#include "Command.h"
#include "Token.h"

#include <optional>
#include <vector>

class Parser {
public:
    static std::optional<Command> parse(const std::vector<Token>& tokens);
    static std::optional<Command> parse_arguments(const CommandArguments& arguments);

private:
    static constexpr std::size_t kUnaryCommandTokenCount = 2;
    static constexpr std::size_t kBinaryCommandTokenCount = 3;
    static constexpr std::size_t kCommandTokenIndex = 0;
    static constexpr std::size_t kFirstArgumentTokenIndex = 1;
    static constexpr std::size_t kSecondArgumentTokenIndex = 2;

    static std::optional<Key> key_from_token(const Token& token);
    static std::optional<Bytes> value_from_token(const Token& token);
};

#endif //PARSER_H
