//
// Created by Mark on 4/8/26.
//

#ifndef EXITSSTATEMENT_H
#define EXITSSTATEMENT_H

#include "Interpreter/Executor/Key.h"
#include "Interpreter/Statements/Statement.h"

class ExistsStatement : public Statement {
private:
    Key key_;
public:
    explicit ExistsStatement(const Key& key) : Statement(StatementType::EXISTS), key_(key) {}

    const Key& key() const {
        return key_;
    }

};


#endif //EXITSSTATEMENT_H
