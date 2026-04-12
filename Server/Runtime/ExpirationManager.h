
#ifndef EXPIRATIONMANAGER_H
#define EXPIRATIONMANAGER_H

#include "Runtime/StorageEngine.h"
#include "Runtime/Key.h"
#include <thread>
#include <chrono>
#include <iostream>
class ExpirationManager {
    private:
        bool shutdown_ = false;
        int sleep_time_ms_ = 10000;
        
    public:

        ExpirationManager(int sleep_time_ms = 10000) : sleep_time_ms_(sleep_time_ms) {

        }

        void expiration_thread(StorageEngine& storage_engine) {
        

            while(!shutdown_) {
                std::unordered_set<Key> possibly_exp_set = storage_engine.possibly_expired();
                for(const auto& key : possibly_exp_set) {
                    storage_engine.prune_if_expired(key, std::chrono::steady_clock::now());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms_));
            }


        }


        void shutdown() {
            shutdown_ = true;
        }
    

    

};


#endif