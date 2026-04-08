//
// Created by Mark on 4/7/26.
//

#ifndef STATEMENT_H
#define STATEMENT_H

#include "StatementType.h"

class Statement {
private:
StatementType type_;
public:
    Statement(StatementType type) : type_(type) {}
    virtual StatementType get_type() {
        return type_;
    }
    virtual ~Statement() = default;
};

#endif //STATEMENT_H
