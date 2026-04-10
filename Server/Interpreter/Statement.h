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
    explicit Statement(StatementType type);
    virtual StatementType get_type() const;
    virtual ~Statement();
};

#endif //STATEMENT_H
