//
// Created by Mark on 4/7/26.
//

#ifndef GETSTATEMENT_H
#define GETSTATEMENT_H
#include "Interpreter/Executor/Key.h"
#include "Interpreter/Statements/Statement.h"
#include "Interpreter/Statements/StatementType.h"

class GetStatement : public Statement{
private:
    Key key_;
public:
    explicit GetStatement(const Key& key) : Statement(StatementType::GET) , key_(key){

    }

    const Key& key() const {
        return key_;
    }

};



#endif //GETSTATEMENT_H
