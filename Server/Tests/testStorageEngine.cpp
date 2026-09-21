#include "StorageEngine.h"

#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <thread>

TEST(StorageEngineTest, SetAndGetRoundTripsStringValue) {
    StorageEngine storage;

    storage.set("user", Value("alice"));

    const std::optional<Value> result = storage.get("user");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes(), "alice");
}

TEST(StorageEngineTest, SetAndGetRoundTripsEmbeddedNullBytes) {
    StorageEngine storage;
    const Bytes bytes{"a\0b", 3};

    storage.set("binary", Value(bytes));

    const std::optional<Value> result = storage.get("binary");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes(), bytes);
    EXPECT_EQ(result->bytes().size(), 3U);
}

TEST(StorageEngineTest, SetOverwritesExistingValue) {
    StorageEngine storage;

    storage.set("answer", Value("41"));
    storage.set("answer", Value("42"));

    const std::optional<Value> result = storage.get("answer");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bytes(), "42");
}

TEST(StorageEngineTest, SetClearsExistingExpirationTracking) {
    StorageEngine storage;
    storage.set("session", Value("old"));
    ASSERT_TRUE(storage.expire("session", std::chrono::hours(1)));
    ASSERT_TRUE(storage.possibly_expired().contains("session"));

    storage.set("session", Value("new"));

    EXPECT_FALSE(storage.possibly_expired().contains("session"));
    ASSERT_TRUE(storage.get("session").has_value());
    EXPECT_EQ(storage.get("session")->bytes(), "new");
}

TEST(StorageEngineTest, DeleteRemovesExistingKey) {
    StorageEngine storage;

    storage.set("session", Value("x"));

    EXPECT_TRUE(storage.del("session"));
    EXPECT_FALSE(storage.exists("session"));
    EXPECT_FALSE(storage.get("session").has_value());
}

TEST(StorageEngineTest, DeleteReturnsFalseForMissingKey) {
    StorageEngine storage;

    EXPECT_FALSE(storage.del("missing"));
}

TEST(StorageEngineTest, DeleteClearsExistingExpirationTracking) {
    StorageEngine storage;
    storage.set("session", Value("value"));
    ASSERT_TRUE(storage.expire("session", std::chrono::hours(1)));
    ASSERT_TRUE(storage.possibly_expired().contains("session"));

    EXPECT_TRUE(storage.del("session"));

    EXPECT_FALSE(storage.possibly_expired().contains("session"));
}

TEST(StorageEngineTest, ExistsReflectsStoredKeys) {
    StorageEngine storage;

    storage.set("pi", Value("3.14"));

    EXPECT_TRUE(storage.exists("pi"));
    EXPECT_FALSE(storage.exists("tau"));
}

TEST(StorageEngineTest, ExpireReturnsFalseForMissingKey) {
    StorageEngine storage;

    EXPECT_FALSE(storage.expire("missing", std::chrono::seconds(5)));
}

TEST(StorageEngineTest, NonPositiveExpireRemovesKeyImmediately) {
    StorageEngine storage;

    storage.set("ephemeral", Value("value"));

    EXPECT_TRUE(storage.expire("ephemeral", std::chrono::seconds(0)));
    EXPECT_FALSE(storage.exists("ephemeral"));
    EXPECT_FALSE(storage.get("ephemeral").has_value());
}

TEST(StorageEngineTest, AbsoluteExpirationInThePastRemovesKeyImmediately) {
    StorageEngine storage;
    storage.set("ephemeral", Value("value"));

    EXPECT_TRUE(storage.expire_at("ephemeral", StorageEngine::Clock::now() - std::chrono::seconds(1)));
    EXPECT_FALSE(storage.exists("ephemeral"));
}

TEST(StorageEngineTest, PositiveExpireKeepsKeyUntilDeadline) {
    StorageEngine storage;

    storage.set("cache", Value("warm"));

    EXPECT_TRUE(storage.expire("cache", std::chrono::seconds(1)));
    EXPECT_TRUE(storage.exists("cache"));
    EXPECT_EQ(storage.size(), 1U);
}

TEST(StorageEngineTest, SizeClearsTrackingForExpiredKeysItPrunes) {
    StorageEngine storage;
    storage.set("expired", Value("value"));
    ASSERT_TRUE(storage.expire("expired", std::chrono::milliseconds(1)));
    ASSERT_TRUE(storage.possibly_expired().contains("expired"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(storage.size(), 0U);

    EXPECT_FALSE(storage.possibly_expired().contains("expired"));
}

TEST(StorageEngineTest, ClearRemovesAllEntries) {
    StorageEngine storage;

    storage.set("a", Value("1"));
    storage.set("b", Value("two"));

    storage.clear();

    EXPECT_EQ(storage.size(), 0U);
    EXPECT_FALSE(storage.exists("a"));
    EXPECT_FALSE(storage.exists("b"));
}
