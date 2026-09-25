#ifndef AOFCOMPACTOR_H
#define AOFCOMPACTOR_H

#include "Persistence/AofConfig.h"
#include <string>
#include <string_view>

class AofCompactor {
public:
    explicit AofCompactor(std::string_view file_path = AofConfig::kDefaultPath);
    void compact() const;

private:
    std::string file_path_;
};


#endif
