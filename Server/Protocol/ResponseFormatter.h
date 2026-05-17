#ifndef RESPONSEFORMATTER_H
#define RESPONSEFORMATTER_H

#include "StatementType.h"
#include "ExecutionResult.h"

#include <string>

class ResponseFormatter {
public:
    static std::string format_error(const std::string& error_message);
    static std::string format_result(StatementType statement_type, const ExecutionResult& result);
};

#endif //RESPONSEFORMATTER_H
