//
// Created by Mark on 4/7/26.
//

#ifndef STATEMENT_H
#define STATEMENT_H

#include "Commands/StatementType.h"

#include <sstream>
#include <string>

class Statement {
private:
StatementType type_;
public:
    explicit Statement(StatementType type);
    virtual StatementType get_type() const;
    virtual bool mutates() const = 0;
    virtual std::string to_string() const = 0;
    virtual ~Statement();

protected:
    template <typename T>
    static std::string serialize_value(const T& value) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    }

    static std::string serialize_value(const std::string& value) {
        return '"' + value + '"';
    }

    static std::string serialize_value(const char value) {
        return std::string("'") + value + "'";
    }
};

#endif //STATEMENT_H
