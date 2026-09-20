//
// Created by Mark on 4/7/26.
//

#ifndef SETSTATEMENT_H
#define SETSTATEMENT_H
#include "Statement.h"
#include "Key.h"
class SetStatement : public Statement{
private:
    Key key_;
    Bytes value_;
public:
    SetStatement(const Key& key, const Bytes& value) : Statement(StatementType::SET) , key_(key), value_(value){

    }

    const Key& key() const {
        return key_;
    }

    const Bytes& value() const {
        return value_;
    }

    bool mutates() const override {
        return true;
    }

    std::optional<std::string> get_key() const override {
        return key_;
    }

    std::string to_string() const override {
        return "SET " + serialize_value(key_) + " " + serialize_value(value_);
    }
};
#endif //SETSTATEMENT_H
