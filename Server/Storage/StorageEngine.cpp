#include "Storage/StorageEngine.h"

#include "Core/Integer.h"

#include <limits>

void StorageEngine::set(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock{mutex_};
    data_.insert_or_assign(key, Entry{value, std::nullopt});
    untrack_deadline(key);
}

void StorageEngine::restore_state(const Key& key, const Value& value,
                                  const std::optional<TimePoint> expires_at) {
    std::lock_guard<std::mutex> lock{mutex_};
    untrack_deadline(key);
    if (expires_at && *expires_at <= Clock::now()) {
        data_.erase(key);
        return;
    }
    data_.insert_or_assign(key, Entry{value, expires_at});
    if (expires_at) {
        track_deadline(key);
    }
}

StorageEngine::IntegerResult StorageEngine::adjust_integer(
    const Key& key, const Bytes& amount, const bool subtract) {
    const auto parsed = parse_integer(amount);
    if (!parsed) return {.error = IntegerError::INVALID_INTEGER};
    return adjust_integer(key, *parsed, subtract);
}

StorageEngine::IntegerResult StorageEngine::adjust_integer(
    const Key& key, const std::int64_t amount, const bool subtract) {
    std::lock_guard<std::mutex> lock{mutex_};
    prune_if_expired_unlocked(key, Clock::now());
    auto it = data_.find(key);
    std::int64_t current = 0;
    if (it != data_.end()) {
        const auto parsed = parse_integer(it->second.value.bytes());
        if (!parsed) return {.error = IntegerError::INVALID_INTEGER};
        current = *parsed;
    }

    // DECRBY cannot negate the smallest signed integer, even if a result might fit.
    if (subtract && amount == std::numeric_limits<std::int64_t>::min()) {
        return {.error = IntegerError::WOULD_OVERFLOW};
    }
    const std::int64_t delta = subtract ? -amount : amount;
    // Would current + delta go past the int64 range? Compared against max - delta
    // (or min - delta) because computing current + delta first would already overflow.
    if ((delta > 0 && current > std::numeric_limits<std::int64_t>::max() - delta) ||
        (delta < 0 && current < std::numeric_limits<std::int64_t>::min() - delta)) {
        return {.error = IntegerError::WOULD_OVERFLOW};
    }
    const std::int64_t next = current + delta;
    const auto expires_at = it == data_.end() ? std::optional<TimePoint>{} : it->second.expires_at;
    if (it == data_.end()) data_.emplace(key, Entry{Value(std::to_string(next)), std::nullopt});
    else it->second.value = Value(std::to_string(next));
    return {.value = next, .expires_at = expires_at};
}

std::optional<Value> StorageEngine::get(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second.value;
}

bool StorageEngine::del(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);
    untrack_deadline(key);
    return data_.erase(key) > 0;
}

bool StorageEngine::exists(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);
    return data_.contains(key);
}

bool StorageEngine::expire(const Key& key, const Duration ttl) {
    return expire_at(key, Clock::now() + ttl).applied;
}

StorageEngine::ExpireResult StorageEngine::expire_at(const Key& key, const TimePoint expires_at) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return {};
    }

    if (expires_at <= now) {
        untrack_deadline(it->first);
        data_.erase(it);
        return {.applied = true};
    }

    track_deadline(key);
    it->second.expires_at = expires_at;
    return {.applied = true, .value = it->second.value};
}

std::int64_t StorageEngine::ttl_milliseconds(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) return -2;
    if (!it->second.expires_at.has_value()) return -1;

    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        it->second.expires_at.value() - now).count();
    return remaining < 0 ? 0 : remaining;
}

std::optional<Value> StorageEngine::persist(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end() || !it->second.expires_at.has_value()) return std::nullopt;

    it->second.expires_at.reset();
    untrack_deadline(key);
    return it->second.value;
}

void StorageEngine::clear() {
    std::lock_guard<std::mutex> lock{mutex_};
    data_.clear();
    expiration_generations_.clear();
    expiration_queue_.clear();
}

std::size_t StorageEngine::size() {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();

    for (auto it = data_.begin(); it != data_.end();) {
        if (is_expired(it->second, now)) {
            untrack_deadline(it->first);
            it = data_.erase(it);
            continue;
        }
        ++it;
    }

    return data_.size();
}

bool StorageEngine::is_expired(const Entry& entry, const TimePoint now) const {
    return entry.expires_at.has_value() && entry.expires_at.value() <= now;
}

void StorageEngine::track_deadline(const Key& key) {
    // A new generation marks every older queue entry for this key as stale.
    const std::uint64_t generation = ++next_expiration_generation_;
    expiration_generations_.insert_or_assign(key, generation);
    expiration_queue_.push_back({key, generation});
    rebuild_expiration_queue_if_mostly_stale();
}

void StorageEngine::rebuild_expiration_queue_if_mostly_stale() {
    // Each tracked key has exactly one current entry in the queue, so a queue more
    // than twice the number of tracked keys is mostly stale entries (for example,
    // from repeated EXPIRE on one key). Rebuild it from the tracked keys to keep
    // memory proportional to them. A rebuild removes at least half the queue, so its
    // cost averages out to constant work per call to track_deadline.
    if (expiration_queue_.size() <= 2 * expiration_generations_.size()) return;
    std::deque<ExpirationCandidate> current;
    for (const auto& [tracked_key, generation] : expiration_generations_) {
        current.push_back({tracked_key, generation});
    }
    expiration_queue_ = std::move(current);
}

void StorageEngine::untrack_deadline(const Key& key) {
    // The key's queue entries stay behind; the sweep skips them as stale.
    expiration_generations_.erase(key);
}

void StorageEngine::prune_if_expired_unlocked(const Key& key, const TimePoint now) {
    const auto it = data_.find(key);
    if (it == data_.end()) {
        untrack_deadline(key);
        return;
    }

    if (is_expired(it->second, now)) {
        untrack_deadline(it->first);
        data_.erase(it);
    }
}

std::size_t StorageEngine::expiration_queue_size() {
    std::lock_guard<std::mutex> lock{mutex_};
    return expiration_queue_.size();
}

std::unordered_set<Key> StorageEngine::keys_with_deadlines() {
    std::lock_guard<std::mutex> lock{mutex_};
    std::unordered_set<Key> keys;
    keys.reserve(expiration_generations_.size());
    for (const auto& [key, generation] : expiration_generations_) keys.insert(key);
    return keys;
}

void StorageEngine::prune_expired_batch(const std::size_t max_candidates, const TimePoint now) {
    std::lock_guard<std::mutex> lock{mutex_};

    for (std::size_t processed = 0;
         processed < max_candidates && !expiration_queue_.empty();
         ++processed) {
        ExpirationCandidate candidate = std::move(expiration_queue_.front());
        expiration_queue_.pop_front();

        const auto generation = expiration_generations_.find(candidate.key);
        if (generation == expiration_generations_.end() ||
            generation->second != candidate.generation) {
            continue;
        }

        prune_if_expired_unlocked(candidate.key, now);
        const auto remaining = expiration_generations_.find(candidate.key);
        if (remaining != expiration_generations_.end() &&
            remaining->second == candidate.generation) {
            expiration_queue_.push_back(std::move(candidate));
        }
    }
}
