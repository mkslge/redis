#include "StorageEngine.h"

void StorageEngine::set(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock{mutex_};
    data_.insert_or_assign(key, Entry{value, std::nullopt});
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
    return data_.erase(key) > 0;
}

bool StorageEngine::exists(const Key& key) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);
    return data_.contains(key);
}

bool StorageEngine::expire(const Key& key, const Duration ttl) {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();
    prune_if_expired_unlocked(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (ttl <= Duration::zero()) {
        possibly_expired_.erase(it->first);
        data_.erase(it);
        return true;
    }

    possibly_expired_.insert(key);
    it->second.expires_at = now + ttl;
    return true;
}

void StorageEngine::clear() {
    std::lock_guard<std::mutex> lock{mutex_};
    data_.clear();
    possibly_expired_.clear();
}

std::size_t StorageEngine::size() {
    std::lock_guard<std::mutex> lock{mutex_};
    const TimePoint now = Clock::now();

    for (auto it = data_.begin(); it != data_.end();) {
        if (is_expired(it->second, now)) {
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
        return;
    }

    if (is_expired(it->second, now)) {
        possibly_expired_.erase(it->first);
        data_.erase(it);
    }
}

std::unordered_set<Key> StorageEngine::possibly_expired() {
    std::lock_guard<std::mutex> lock{mutex_};
    return possibly_expired_;
}
