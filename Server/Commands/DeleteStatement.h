//
// Created by Mark on 4/7/26.
//

#ifndef DELETESTATEMENT_H
#define DELETESTATEMENT_H
#include "Commands/Statement.h"
#include "Commands/StatementType.h"
#include "Runtime/Key.h"

class DeleteStatement : public Statement{
private:
    Key key_;
public:
    explicit DeleteStatement(const Key& key) : Statement(StatementType::DELETE) , key_(key){

    }

    const Key& key() const {
        return key_;
    }

    bool mutates() const override {
        return true;
    }

    std::string to_string() const override {
        return "DEL " + serialize_value(key_);
    }

};
#endif //DELETESTATEMENT_H
