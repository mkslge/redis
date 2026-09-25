#ifndef LOGCOMPACTOR_H
#define LOGCOMPACTOR_H

#include "Persistence/LogConfig.h"
#include <string>
#include <string_view>

class LogCompactor {
public:
    explicit LogCompactor(std::string_view file_path = LogConfig::kDefaultAofPath);
    void compact() const;

private:
    std::string file_path_;
};


#endif
