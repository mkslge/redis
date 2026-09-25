#ifndef VALUE_H
#define VALUE_H

#include "Core/Bytes.h"

#include <utility>

class Value {
public:
    explicit Value(Bytes bytes) : bytes_(std::move(bytes)) {}
    Value(const char* value) : bytes_(value) {}

    const Bytes& bytes() const {
        return bytes_;
    }

    bool operator==(const Value& other) const {
        return bytes_ == other.bytes_;
    }

private:
    Bytes bytes_;
};

#endif //VALUE_H
