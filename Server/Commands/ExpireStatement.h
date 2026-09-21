//
// Created by Mark on 4/8/26.
//

#ifndef EXPIRESTATEMENT_H
#define EXPIRESTATEMENT_H

#include "Statement.h"
#include "Key.h"

#include <chrono>
#include <cstdint>

class ExpireStatement : public Statement {
private:
    Key key_;
    std::chrono::system_clock::time_point expires_at_;
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    ExpireStatement(const Key& key, const int expire_time)
        : ExpireStatement(key, Clock::now() + std::chrono::seconds(expire_time)) {}

    ExpireStatement(const Key& key, const TimePoint expires_at)
        : Statement(StatementType::EXPIRE), key_(key), expires_at_(expires_at) {}

    const Key& key() const {
        return key_;
    }

    TimePoint expires_at() const {
        return expires_at_;
    }

    std::int64_t expires_at_unix_milliseconds() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            expires_at_.time_since_epoch()).count();
    }

    bool mutates() const override {
        return true;
    }

    CommandArguments arguments() const override {
        return {"PEXPIREAT", key_, std::to_string(expires_at_unix_milliseconds())};
    }


};


#endif //EXPIRESTATEMENT_H
