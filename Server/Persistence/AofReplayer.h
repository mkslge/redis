#ifndef AOFREPLAYER_H
#define AOFREPLAYER_H

#include "Commands/Executor.h"
#include "Persistence/AofConfig.h"

#include <string>

class AofReplayer {
public:
    explicit AofReplayer(const std::string& file_path = std::string(AofConfig::kDefaultPath));
    // Re-executes every AOF record; throws on malformed, non-mutating, or failing records.
    void replay(Executor& executor) const;

private:
    std::string file_path_;
};

#endif //AOFREPLAYER_H
