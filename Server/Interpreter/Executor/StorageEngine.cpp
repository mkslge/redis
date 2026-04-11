#include "Interpreter/Executor/StorageEngine.h"

void StorageEngine::set(const Key& key, const Value& value) {
    data_.insert_or_assign(key, Entry{value, std::nullopt});
}

std::optional<Value> StorageEngine::get(const Key& key) {
    const TimePoint now = Clock::now();
    prune_if_expired(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second.value;
}

bool StorageEngine::del(const Key& key) {
    const TimePoint now = Clock::now();
    prune_if_expired(key, now);
    return data_.erase(key) > 0;
}

bool StorageEngine::exists(const Key& key) {
    const TimePoint now = Clock::now();
    prune_if_expired(key, now);
    return data_.contains(key);
}

bool StorageEngine::expire(const Key& key, const Duration ttl) {
    const TimePoint now = Clock::now();
    prune_if_expired(key, now);

    const auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (ttl <= Duration::zero()) {
        data_.erase(it);
        return true;
    }

    it->second.expires_at = now + ttl;
    return true;
}

void StorageEngine::clear() {
    data_.clear();
}

std::size_t StorageEngine::size() {
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
    const auto it = data_.find(key);
    if (it == data_.end()) {
        return;
    }

    if (is_expired(it->second, now)) {
        data_.erase(it);
    }
}
