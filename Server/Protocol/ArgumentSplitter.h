#ifndef ARGUMENTSPLITTER_H
#define ARGUMENTSPLITTER_H

#include "Core/CommandArguments.h"

#include <optional>
#include <string_view>

// Splits a command line into byte-string arguments using redis-cli quoting rules.
class ArgumentSplitter {
public:
    // Returns nullopt for unbalanced quotes; returns an empty vector for a blank line.
    static std::optional<CommandArguments> split(std::string_view line);
};

#endif //ARGUMENTSPLITTER_H
