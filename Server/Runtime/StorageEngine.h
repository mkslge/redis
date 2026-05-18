#ifndef STORAGEENGINE_H
#define STORAGEENGINE_H

#include "Key.h"
#include "Value.h"

#include <chrono>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <memory>

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
    std::unordered_set<Key> possibly_expired();
    void prune_if_expired(const Key& key, TimePoint now);
private:
    struct Entry {
        Value value;
        std::optional<TimePoint> expires_at;
    };

    bool is_expired(const Entry& entry, TimePoint now) const;
    std::mutex* get_mutex(const Key& key); //returns keys mutex, creates a new one if it doesnt exist yet
    

    std::unordered_map<Key, Entry> data_;
    std::unordered_map<Key, std::unique_ptr<std::mutex>> mutexes_;
    std::unordered_set<Key> possibly_expired_;
};

#endif //STORAGEENGINE_H
