#ifndef RESPONSEFORMATTER_H
#define RESPONSEFORMATTER_H

#include "Command.h"
#include "ExecutionResult.h"

#include <string>

class ResponseFormatter {
public:
    static std::string format_error(const std::string& error_message);
    static std::string format_result(const Command& command, const ExecutionResult& result);
};

#endif //RESPONSEFORMATTER_H
