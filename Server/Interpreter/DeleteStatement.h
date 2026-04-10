//
// Created by Mark on 4/7/26.
//

#ifndef DELETESTATEMENT_H
#define DELETESTATEMENT_H
#include "Statement.h"
#include "StatementType.h"

template <typename K>
class DeleteStatement : public Statement{
private:
    K key_;
public:
    DeleteStatement(const K& key) : Statement(StatementType::DELETE) , key_(key){

    }

    const K& key() const {
        return key_;
    }

};
#endif //DELETESTATEMENT_H
