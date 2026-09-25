#ifndef STORAGEENGINE_H
#define STORAGEENGINE_H

#include "Core/Key.h"
#include "Core/Value.h"

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

    enum class IntegerError { NONE, INVALID_INTEGER, WOULD_OVERFLOW };
    struct IntegerResult {
        IntegerError error{IntegerError::NONE};
        std::int64_t value{0};
        std::optional<TimePoint> expires_at;
    };
    struct ExpireResult {
        bool applied{false};
        std::optional<Value> value;
    };

    // Command operations. Each prunes the key first if its deadline has passed.
    void set(const Key& key, const Value& value);
    std::optional<Value> get(const Key& key);
    bool del(const Key& key);
    bool exists(const Key& key);
    IntegerResult adjust_integer(const Key& key, std::int64_t amount, bool subtract);
    IntegerResult adjust_integer(const Key& key, const Bytes& amount, bool subtract);
    // Sets an absolute deadline; a past deadline deletes the key. Returns the value it now expires with.
    ExpireResult expire_at(const Key& key, TimePoint expires_at);
    std::int64_t ttl_milliseconds(const Key& key);
    // Removes a deadline; returns the key's value only if a deadline was removed.
    std::optional<Value> persist(const Key& key);

    // AOF restore: replaces a key's complete state, including its deadline.
    void restore_state(const Key& key, const Value& value, std::optional<TimePoint> expires_at);

    // Expiration maintenance, run periodically by the server event loop.
    void prune_expired_batch(std::size_t max_candidates, TimePoint now);

    // Diagnostics and test support.
    bool expire(const Key& key, Duration ttl);
    std::unordered_set<Key> possibly_expired();
    std::size_t size();
    void clear();
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
