#ifndef LOGRUNNER_H
#define LOGRUNNER_H

#include "Commands/Executor.h"
#include "Persistence/LogConfig.h"

#include <string>

class LogRunner {
public:
    explicit LogRunner(const std::string& file_path = std::string(LogConfig::kDefaultAofPath));
    // Re-executes every AOF record; throws on malformed, non-mutating, or failing records.
    void run_log(Executor& executor) const;

private:
    std::string file_path_;
};

#endif //LOGRUNNER_H
