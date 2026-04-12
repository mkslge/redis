#ifndef LOGRUNNER_H
#define LOGRUNNER_H

#include "Interpreter/Runtime/CommandHandler.h"
#include "Log/LogConfig.h"

#include <string>

class LogRunner {
public:
    explicit LogRunner(const std::string& file_path = std::string(LogConfig::kDefaultAofPath));

    const std::string& file_path() const;
    void run_log(CommandHandler& command_handler) const;

private:
    std::string file_path_;
};

#endif //LOGRUNNER_H
