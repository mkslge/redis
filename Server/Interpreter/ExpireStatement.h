//
// Created by Mark on 4/8/26.
//

#ifndef EXPIRESTATEMENT_H
#define EXPIRESTATEMENT_H

#include "Statement.h"

template<typename K>
class ExpireStatement : Statement {
private:
    K key_;
    int expire_time_{};
public:
    ExpireStatement(const K& key, int expire_time) : Statement(StatementType::EXPIRE), key_(key), expire_time_(expire_time) {

    }


};


#endif //EXPIRESTATEMENT_H
