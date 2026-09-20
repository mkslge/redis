//
// Created by Mark on 4/7/26.
//

#ifndef DELETESTATEMENT_H
#define DELETESTATEMENT_H
#include "Statement.h"
#include "StatementType.h"
#include "Key.h"
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

    CommandArguments arguments() const override { return {"DEL", key_}; }

};
#endif //DELETESTATEMENT_H
