#ifndef STORAGEENGINE_H
#define STORAGEENGINE_H

#include "Interpreter/Executor/Key.h"
#include "Interpreter/Executor/Value.h"

#include <chrono>
#include <optional>
#include <unordered_map>

class StorageEngine {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using TimePoint = Clock::time_point;

    void set(const Key& key, const Value& value);
    std::optional<Value> get(const Key& key);
    bool del(const Key& key);
    bool exists(const Key& key);
    bool expire(const Key& key, Duration ttl);
    void clear();
    std::size_t size();

private:
    struct Entry {
        Value value;
        std::optional<TimePoint> expires_at;
    };

    bool is_expired(const Entry& entry, TimePoint now) const;
    void prune_if_expired(const Key& key, TimePoint now);

    std::unordered_map<Key, Entry> data_;
};

#endif //STORAGEENGINE_H
