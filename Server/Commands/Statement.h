//
// Created by Mark on 4/7/26.
//

#ifndef STATEMENT_H
#define STATEMENT_H

#include "StatementType.h"
#include "Bytes.h"

class Statement {
private:
StatementType type_;
public:
    explicit Statement(StatementType type);
    virtual StatementType get_type() const;
    virtual bool mutates() const = 0;
    virtual CommandArguments arguments() const = 0;
    virtual ~Statement();
};

#endif //STATEMENT_H
