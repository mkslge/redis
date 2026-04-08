//
// Created by Mark on 4/7/26.
//

#ifndef SETSTATEMENT_H
#define SETSTATEMENT_H
#include "Statement.h"
template <typename K, typename V>
class SetStatement : public Statement{
private:
    K key_;
    V value_;
public:
    SetStatement(const K& key, const V& value) : Statement(StatementType::GET) , key_(key), value_(value){

    }
};
#endif //SETSTATEMENT_H
