#include "Runtime/ExpirationManager.h"
#include "Runtime/StorageEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace {

class ExpirationManagerThreadHarness {
public:
    explicit ExpirationManagerThreadHarness(const int sleep_time_ms)
        : manager_(sleep_time_ms), thread_(&ExpirationManager::expiration_thread, &manager_, std::ref(storage_)) {}

    ~ExpirationManagerThreadHarness() {
        stop();
    }

    StorageEngine& storage() {
        return storage_;
    }

    void stop() {
        if (!stopped_) {
            manager_.shutdown();
            if (thread_.joinable()) {
                thread_.join();
            }
            stopped_ = true;
        }
    }

private:
    StorageEngine storage_;
    ExpirationManager manager_;
    std::thread thread_;
    bool stopped_{false};
};

} // namespace

TEST(ExpirationManagerTest, BackgroundThreadRemovesExpiredKey) {
    ExpirationManagerThreadHarness harness(5);

    harness.storage().set("session", Value("token"));
    ASSERT_TRUE(harness.storage().expire("session", std::chrono::milliseconds(20)));

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    harness.stop();

    EXPECT_FALSE(harness.storage().exists("session"));
    EXPECT_FALSE(harness.storage().get("session").has_value());
}

TEST(ExpirationManagerTest, BackgroundThreadDoesNotRemoveKeyBeforeDeadline) {
    ExpirationManagerThreadHarness harness(5);

    harness.storage().set("cache", Value("warm"));
    ASSERT_TRUE(harness.storage().expire("cache", std::chrono::milliseconds(100)));

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    harness.stop();

    EXPECT_TRUE(harness.storage().exists("cache"));
    ASSERT_TRUE(harness.storage().get("cache").has_value());
    EXPECT_EQ(harness.storage().get("cache")->get<std::string>(), "warm");
}

TEST(ExpirationManagerTest, BackgroundThreadRemovesOnlyKeysWhoseDeadlinesPassed) {
    ExpirationManagerThreadHarness harness(5);

    harness.storage().set("short", Value(1));
    harness.storage().set("long", Value(2));
    ASSERT_TRUE(harness.storage().expire("short", std::chrono::milliseconds(20)));
    ASSERT_TRUE(harness.storage().expire("long", std::chrono::milliseconds(100)));

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    harness.stop();

    EXPECT_FALSE(harness.storage().exists("short"));
    EXPECT_TRUE(harness.storage().exists("long"));
}
