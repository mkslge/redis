//
// Created by Mark on 4/10/26.
//

#include "Commands/Statement.h"

Statement::Statement(StatementType type) : type_(type) {}

StatementType Statement::get_type() const {
    return type_;
}

Statement::~Statement() = default;
