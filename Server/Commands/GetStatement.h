//
// Created by Mark on 4/7/26.
//

#ifndef GETSTATEMENT_H
#define GETSTATEMENT_H
#include "Statement.h"
#include "StatementType.h"
#include "Key.h"
class GetStatement : public Statement{
private:
    Key key_;
public:
    explicit GetStatement(const Key& key) : Statement(StatementType::GET) , key_(key){

    }

    const Key& key() const {
        return key_;
    }

    bool mutates() const override {
        return false;
    }

    CommandArguments arguments() const override { return {"GET", key_}; }

};



#endif //GETSTATEMENT_H
