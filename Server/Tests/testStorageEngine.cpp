#include "Runtime/StorageEngine.h"

#include "gtest/gtest.h"

#include <chrono>
#include <string>

TEST(StorageEngineTest, SetAndGetRoundTripsStringValue) {
    StorageEngine storage;

    storage.set("user", Value("alice"));

    const std::optional<Value> result = storage.get("user");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<std::string>());
    EXPECT_EQ(result->get<std::string>(), "alice");
}

TEST(StorageEngineTest, SetOverwritesExistingValue) {
    StorageEngine storage;

    storage.set("answer", Value(41));
    storage.set("answer", Value(42));

    const std::optional<Value> result = storage.get("answer");

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is<int>());
    EXPECT_EQ(result->get<int>(), 42);
}

TEST(StorageEngineTest, DeleteRemovesExistingKey) {
    StorageEngine storage;

    storage.set("session", Value('x'));

    EXPECT_TRUE(storage.del("session"));
    EXPECT_FALSE(storage.exists("session"));
    EXPECT_FALSE(storage.get("session").has_value());
}

TEST(StorageEngineTest, DeleteReturnsFalseForMissingKey) {
    StorageEngine storage;

    EXPECT_FALSE(storage.del("missing"));
}

TEST(StorageEngineTest, ExistsReflectsStoredKeys) {
    StorageEngine storage;

    storage.set("pi", Value(3.14));

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

TEST(StorageEngineTest, PositiveExpireKeepsKeyUntilDeadline) {
    StorageEngine storage;

    storage.set("cache", Value("warm"));

    EXPECT_TRUE(storage.expire("cache", std::chrono::seconds(1)));
    EXPECT_TRUE(storage.exists("cache"));
    EXPECT_EQ(storage.size(), 1U);
}

TEST(StorageEngineTest, ClearRemovesAllEntries) {
    StorageEngine storage;

    storage.set("a", Value(1));
    storage.set("b", Value("two"));

    storage.clear();

    EXPECT_EQ(storage.size(), 0U);
    EXPECT_FALSE(storage.exists("a"));
    EXPECT_FALSE(storage.exists("b"));
}
