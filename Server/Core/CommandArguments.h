#ifndef COMMANDARGUMENTS_H
#define COMMANDARGUMENTS_H

#include "Core/Bytes.h"

#include <vector>

// One command as a list of byte strings, name first: {"SET", "key", "value"}.
using CommandArguments = std::vector<Bytes>;

#endif //COMMANDARGUMENTS_H
