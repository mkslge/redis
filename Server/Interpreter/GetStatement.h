//
// Created by Mark on 4/7/26.
//

#ifndef GETSTATEMENT_H
#define GETSTATEMENT_H
#include "Statement.h"
#include "StatementType.h"

template <typename K>
class GetStatement : public Statement{
private:
    K key_;
public:
    GetStatement(const K& key) : Statement(StatementType::GET) , key_(key){

    }

    const K& key() const {
        return key_;
    }

};



#endif //GETSTATEMENT_H
