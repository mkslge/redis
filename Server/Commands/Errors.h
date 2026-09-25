#ifndef ERRORS_H
#define ERRORS_H

#include <string>
#include <string_view>

// Every error message a client can receive. They are part of the wire protocol,
// and tests assert their exact text, we should only change them only deliberately.
namespace Errors {
// Request line could not be split into arguments.
inline const std::string kUnbalancedQuotes = "unbalanced quotes in request";

// Arguments do not form a command.
inline const std::string kEmptyCommand = "empty command";
inline const std::string kUnknownCommand = "unknown command";
inline const std::string kInvalidExpireTime = "invalid expire time in 'expire' command";
// command_name is the lowercase command name, e.g. "get".
inline std::string wrong_argument_count(const std::string_view command_name) {
    return "wrong number of arguments for '" + std::string(command_name) + "' command";
}

// Shared by parsing (EXPIRE seconds) and execution (INCRBY amounts, stored values).
inline const std::string kNotAnInteger = "value is not an integer or out of range";

// Command ran but could not complete.
inline const std::string kIntegerOverflow = "increment or decrement would overflow";
}

#endif //ERRORS_H
