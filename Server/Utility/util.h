//
// Created by Mark on 4/5/26.
//

#ifndef UTIL_H
#define UTIL_H
#include <cctype>
#include <string>


namespace util {
    std::string to_lower(std::string& data) {
        std::string cpy;
        for (auto& c : data) {
            cpy += std::tolower(static_cast<unsigned char>(c));
        }
        return cpy;
    }



}
#endif //UTIL_H
