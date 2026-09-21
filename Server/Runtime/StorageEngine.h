#ifndef STORAGEENGINE_H
#define STORAGEENGINE_H

#include "Key.h"
#include "Value.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

class StorageEngine {
public:
    using Clock = std::chrono::system_clock;
    using Duration = Clock::duration;
    using TimePoint = Clock::time_point;

    void set(const Key& key, const Value& value);
    std::optional<Value> get(const Key& key);
    bool del(const Key& key);
    bool exists(const Key& key);
    bool expire(const Key& key, Duration ttl);
    bool expire_at(const Key& key, TimePoint expires_at);
    std::int64_t ttl_milliseconds(const Key& key);
    bool persist(const Key& key);
    void clear();
    std::size_t size();
    std::unordered_set<Key> possibly_expired();
    void prune_if_expired(const Key& key, TimePoint now);
    void prune_expired_batch(std::size_t max_candidates, TimePoint now);
private:
    struct Entry {
        Value value;
        std::optional<TimePoint> expires_at;
    };

    struct ExpirationCandidate {
        Key key;
        std::uint64_t generation;
    };

    bool is_expired(const Entry& entry, TimePoint now) const;
    void prune_if_expired_unlocked(const Key& key, TimePoint now);

    std::mutex mutex_;
    std::unordered_map<Key, Entry> data_;
    std::unordered_map<Key, std::uint64_t> expiration_generations_;
    std::deque<ExpirationCandidate> expiration_queue_;
    std::uint64_t next_expiration_generation_{0};
};

#endif //STORAGEENGINE_H
