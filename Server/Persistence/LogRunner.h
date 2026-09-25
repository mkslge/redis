#ifndef LOGRUNNER_H
#define LOGRUNNER_H

#include "App/CommandProcessor.h"
#include "Persistence/LogConfig.h"

#include <string>

class LogRunner {
public:
    explicit LogRunner(const std::string& file_path = std::string(LogConfig::kDefaultAofPath));
    void run_log(CommandProcessor& command_processor) const;

private:
    std::string file_path_;
};

#endif //LOGRUNNER_H
