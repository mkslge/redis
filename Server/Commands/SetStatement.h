//
// Created by Mark on 4/7/26.
//

#ifndef SETSTATEMENT_H
#define SETSTATEMENT_H
#include "Commands/Statement.h"
#include "Runtime/Key.h"
template <typename V>
class SetStatement : public Statement{
private:
    Key key_;
    V value_;
public:
    SetStatement(const Key& key, const V& value) : Statement(StatementType::SET) , key_(key), value_(value){

    }

    const Key& key() const {
        return key_;
    }

    const V& value() const {
        return value_;
    }

    bool mutates() const override {
        return true;
    }

    std::string to_string() const override {
        return "SET " + serialize_value(key_) + " " + serialize_value(value_);
    }
};
#endif //SETSTATEMENT_H
