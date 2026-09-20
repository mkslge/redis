//
// Created by Mark on 4/8/26.
//

#ifndef EXPIRESTATEMENT_H
#define EXPIRESTATEMENT_H

#include "Statement.h"
#include "Key.h"
class ExpireStatement : public Statement {
private:
    Key key_;
    int expire_time_{};
public:
    ExpireStatement(const Key& key, int expire_time) : Statement(StatementType::EXPIRE), key_(key), expire_time_(expire_time) {

    }

    const Key& key() const {
        return key_;
    }

    int expire_time() const {
        return expire_time_;
    }

    bool mutates() const override {
        return true;
    }

    CommandArguments arguments() const override {
        return {"EXPIRE", key_, std::to_string(expire_time_)};
    }


};


#endif //EXPIRESTATEMENT_H
