#ifndef AOFLOGGER_H
#define AOFLOGGER_H

#include "Log/LogConfig.h"

#include <fstream>
#include <string>

class AOFLogger {
public:
    explicit AOFLogger(const std::string& file_path = std::string(LogConfig::kDefaultAofPath));
    void enqueue(const std::string& log_entry);

private:
    std::ofstream stream_;
};

#endif //AOFLOGGER_H
