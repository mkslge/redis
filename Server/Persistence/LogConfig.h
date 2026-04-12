#ifndef LOGCONFIG_H
#define LOGCONFIG_H

#include <string_view>

namespace LogConfig {
inline constexpr std::string_view kDefaultAofPath = "Persistence/appendonlylog.txt";
}

#endif //LOGCONFIG_H
