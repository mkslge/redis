#include "StorageEngine.h"

void StorageEngine::set(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock{mutex_};
    data_.insert_or_assign(key, Entry{value, std::nullopt});
    expiration_generations_.erase(key);
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
    expiration_generations_.erase(key);
    return data_.erase(key) > 0;
}

bool StorageEngine::exists(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);
    return data_.contains(key);
}

bool StorageEngine::expire(const Key& key, const Duration ttl) {
    return expire_at(key, Clock::now() + ttl);
}

bool StorageEngine::expire_at(const Key& key, const TimePoint expires_at) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (expires_at <= now) {
        expiration_generations_.erase(it->first);
        data_.erase(it);
        return true;
    }

    const std::uint64_t generation = ++next_expiration_generation_;
    expiration_generations_.insert_or_assign(key, generation);
    expiration_queue_.push_back({key, generation});
    it->second.expires_at = expires_at;
    return true;
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

bool StorageEngine::persist(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end() || !it->second.expires_at.has_value()) return false;

    it->second.expires_at.reset();
    expiration_generations_.erase(key);
    return true;
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
            expiration_generations_.erase(it->first);
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

void StorageEngine::prune_if_expired(const Key& key, const TimePoint now) {
    std::lock_guard<std::mutex> lock{mutex_};
    prune_if_expired_unlocked(key, now);
}

void StorageEngine::prune_if_expired_unlocked(const Key& key, const TimePoint now) {
    const auto it = data_.find(key);
    if (it == data_.end()) {
        expiration_generations_.erase(key);
        return;
    }

    if (is_expired(it->second, now)) {
        expiration_generations_.erase(it->first);
        data_.erase(it);
    }
}

std::unordered_set<Key> StorageEngine::possibly_expired() {
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
