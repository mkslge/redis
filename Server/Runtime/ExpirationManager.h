#ifndef EXPIRATIONMANAGER_H
#define EXPIRATIONMANAGER_H

#include "Runtime/StorageEngine.h"

class ExpirationManager {
private:
    bool shutdown_ = false;
    int sleep_time_ms_ = 10000;

public:
    explicit ExpirationManager(int sleep_time_ms = 10000);
    void expiration_thread(StorageEngine& storage_engine);
    void shutdown();
};

#endif
