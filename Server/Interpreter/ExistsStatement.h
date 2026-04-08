//
// Created by Mark on 4/8/26.
//

#ifndef EXITSSTATEMENT_H
#define EXITSSTATEMENT_H

#include "Statement.h"

template<typename K>
class ExistsStatement : public Statement {
private:
    K key_;
public:
    ExistsStatement(const K& key) : Statement(StatementType::EXISTS), key_(key) {}

};


#endif //EXITSSTATEMENT_H
