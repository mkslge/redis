#ifndef PARSER_H
#define PARSER_H

#include "Command.h"

#include <optional>
#include <string>
#include <variant>

struct ParseError {
    std::string message;
};

using ParseResult = std::variant<Command, ParseError>;

class Parser {
public:
    // Client commands: relative EXPIRE is accepted; internal AOF commands are not.
    static ParseResult parse_request(const CommandArguments& arguments);
    // AOF records: absolute PEXPIREAT and SETSTATE are accepted; relative EXPIRE is not.
    static std::optional<Command> parse_arguments(const CommandArguments& arguments);
};

#endif //PARSER_H
