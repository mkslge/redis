//
// Created by Mark on 4/10/26.
//

#include "util.h"

#include <cctype>

namespace util {
    std::string to_lowercase(std::string& data) {
        std::string cpy;
        for (auto& c : data) {
            cpy += std::tolower(static_cast<unsigned char>(c));
        }
        return cpy;
    }
}
