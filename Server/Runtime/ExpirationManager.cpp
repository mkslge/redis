#include "Runtime/ExpirationManager.h"

#include "Runtime/Key.h"

#include <chrono>
#include <thread>
#include <unordered_set>

ExpirationManager::ExpirationManager(const int sleep_time_ms) : sleep_time_ms_(sleep_time_ms) {}

void ExpirationManager::expiration_thread(StorageEngine& storage_engine) {
    while (!shutdown_) {
        const std::unordered_set<Key> possibly_exp_set = storage_engine.possibly_expired();
        for (const auto& key : possibly_exp_set) {
            storage_engine.prune_if_expired(key, std::chrono::steady_clock::now());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms_));
    }
}

void ExpirationManager::shutdown() {
    shutdown_ = true;
}
