#include "Storage/StorageEngine.h"

#include "gtest/gtest.h"

#include <chrono>
#include <cstdint>
#include <limits>
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
    ASSERT_TRUE(storage.keys_with_deadlines().contains("session"));

    storage.set("session", Value("new"));

    EXPECT_FALSE(storage.keys_with_deadlines().contains("session"));
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
    ASSERT_TRUE(storage.keys_with_deadlines().contains("session"));

    EXPECT_TRUE(storage.del("session"));

    EXPECT_FALSE(storage.keys_with_deadlines().contains("session"));
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

    EXPECT_TRUE(storage.expire_at("ephemeral", StorageEngine::Clock::now() - std::chrono::seconds(1)).applied);
    EXPECT_FALSE(storage.exists("ephemeral"));
}

TEST(StorageEngineTest, PositiveExpireKeepsKeyUntilDeadline) {
    StorageEngine storage;

    storage.set("cache", Value("warm"));

    EXPECT_TRUE(storage.expire("cache", std::chrono::seconds(1)));
    EXPECT_TRUE(storage.exists("cache"));
    EXPECT_EQ(storage.size(), 1U);
}

TEST(StorageEngineTest, TtlMillisecondsUsesRedisSentinelValues) {
    StorageEngine storage;

    EXPECT_EQ(storage.ttl_milliseconds("missing"), -2);
    storage.set("permanent", Value("value"));
    EXPECT_EQ(storage.ttl_milliseconds("permanent"), -1);
}

TEST(StorageEngineTest, TtlMillisecondsReturnsRemainingLifetime) {
    StorageEngine storage;
    storage.set("session", Value("value"));
    ASSERT_TRUE(storage.expire("session", std::chrono::seconds(30)));

    const std::int64_t ttl = storage.ttl_milliseconds("session");

    EXPECT_GT(ttl, 29'000);
    EXPECT_LE(ttl, 30'000);
}

TEST(StorageEngineTest, PersistRemovesExpirationButKeepsValue) {
    StorageEngine storage;
    storage.set("session", Value("value"));
    ASSERT_TRUE(storage.expire("session", std::chrono::seconds(30)));

    EXPECT_TRUE(storage.persist("session"));
    EXPECT_EQ(storage.ttl_milliseconds("session"), -1);
    EXPECT_TRUE(storage.exists("session"));
    EXPECT_FALSE(storage.keys_with_deadlines().contains("session"));
}

TEST(StorageEngineTest, PersistReturnsFalseWithoutAnExpiration) {
    StorageEngine storage;
    storage.set("permanent", Value("value"));

    EXPECT_FALSE(storage.persist("missing"));
    EXPECT_FALSE(storage.persist("permanent"));
}

TEST(StorageEngineTest, SizeClearsTrackingForExpiredKeysItPrunes) {
    StorageEngine storage;
    storage.set("expired", Value("value"));
    ASSERT_TRUE(storage.expire("expired", std::chrono::milliseconds(1)));
    ASSERT_TRUE(storage.keys_with_deadlines().contains("expired"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_EQ(storage.size(), 0U);

    EXPECT_FALSE(storage.keys_with_deadlines().contains("expired"));
}

TEST(StorageEngineTest, ExpirationBatchLimitsWorkPerSweep) {
    StorageEngine storage;
    const auto deadline = StorageEngine::Clock::now() + std::chrono::seconds(10);
    for (const std::string key : {"one", "two", "three"}) {
        storage.set(key, Value("value"));
        ASSERT_TRUE(storage.expire_at(key, deadline).applied);
    }

    storage.prune_expired_batch(2, deadline);

    EXPECT_EQ(storage.keys_with_deadlines().size(), 1U);
}

TEST(StorageEngineTest, ExpirationBatchIgnoresStaleQueueEntries) {
    StorageEngine storage;
    const auto now = StorageEngine::Clock::now();
    storage.set("session", Value("old"));
    ASSERT_TRUE(storage.expire_at("session", now + std::chrono::seconds(1)).applied);
    storage.set("session", Value("new"));
    ASSERT_TRUE(storage.expire_at("session", now + std::chrono::seconds(10)).applied);

    storage.prune_expired_batch(2, now + std::chrono::seconds(2));

    ASSERT_TRUE(storage.get("session").has_value());
    EXPECT_EQ(storage.get("session")->bytes(), "new");
    EXPECT_TRUE(storage.keys_with_deadlines().contains("session"));
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

namespace {
constexpr std::int64_t kInt64Max = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kInt64Min = std::numeric_limits<std::int64_t>::min();

using IntegerError = StorageEngine::IntegerError;
} // namespace

TEST(StorageEngineTest, AdjustIntegerTreatsMissingKeyAsZero) {
    StorageEngine storage;

    const auto added = storage.adjust_integer("counter", std::int64_t{5}, false);
    EXPECT_EQ(added.error, IntegerError::NONE);
    EXPECT_EQ(added.value, 5);
    EXPECT_FALSE(added.expires_at.has_value());
    EXPECT_EQ(storage.get("counter")->bytes(), "5");

    const auto subtracted = storage.adjust_integer("other", std::int64_t{3}, true);
    EXPECT_EQ(subtracted.value, -3);
    EXPECT_EQ(storage.get("other")->bytes(), "-3");
}

TEST(StorageEngineTest, AdjustIntegerParsesAmountBytesStrictly) {
    StorageEngine storage;
    storage.set("counter", Value("10"));

    EXPECT_EQ(storage.adjust_integer("counter", Bytes("7"), false).value, 17);
    for (const Bytes amount : {"abc", "+5", "007", "-0", "1.5", ""}) {
        SCOPED_TRACE(amount);
        EXPECT_EQ(storage.adjust_integer("counter", amount, false).error, IntegerError::INVALID_INTEGER);
    }
    EXPECT_EQ(storage.get("counter")->bytes(), "17");
}

TEST(StorageEngineTest, AdjustIntegerRejectsNonIntegerValueWithoutChangingIt) {
    StorageEngine storage;
    storage.set("name", Value("mark"));

    EXPECT_EQ(storage.adjust_integer("name", std::int64_t{1}, false).error, IntegerError::INVALID_INTEGER);
    EXPECT_EQ(storage.get("name")->bytes(), "mark");
}

TEST(StorageEngineTest, AdjustIntegerRejectsOverflowWithoutChangingValue) {
    StorageEngine storage;
    storage.set("high", Value(std::to_string(kInt64Max)));
    storage.set("low", Value(std::to_string(kInt64Min)));

    EXPECT_EQ(storage.adjust_integer("high", std::int64_t{1}, false).error, IntegerError::WOULD_OVERFLOW);
    EXPECT_EQ(storage.adjust_integer("low", std::int64_t{1}, true).error, IntegerError::WOULD_OVERFLOW);
    EXPECT_EQ(storage.get("high")->bytes(), std::to_string(kInt64Max));
    EXPECT_EQ(storage.get("low")->bytes(), std::to_string(kInt64Min));

    // Results exactly at the limits are fine.
    EXPECT_EQ(storage.adjust_integer("high", std::int64_t{1}, true).value, kInt64Max - 1);
    EXPECT_EQ(storage.adjust_integer("zero", kInt64Min, false).value, kInt64Min);
}

TEST(StorageEngineTest, AdjustIntegerCannotSubtractInt64Min) {
    StorageEngine storage;
    storage.set("counter", Value("-1"));

    // -1 - INT64_MIN would fit, but negating INT64_MIN itself overflows, so it is
    // rejected regardless of the current value.
    EXPECT_EQ(storage.adjust_integer("counter", kInt64Min, true).error, IntegerError::WOULD_OVERFLOW);
    EXPECT_EQ(storage.get("counter")->bytes(), "-1");
}

TEST(StorageEngineTest, AdjustIntegerKeepsExistingDeadline) {
    StorageEngine storage;
    storage.set("counter", Value("1"));
    const StorageEngine::TimePoint deadline = StorageEngine::Clock::now() + std::chrono::hours(1);
    ASSERT_TRUE(storage.expire_at("counter", deadline).applied);

    const auto result = storage.adjust_integer("counter", std::int64_t{1}, false);

    EXPECT_EQ(result.value, 2);
    EXPECT_EQ(result.expires_at, deadline);
    EXPECT_GT(storage.ttl_milliseconds("counter"), 0);
    EXPECT_TRUE(storage.keys_with_deadlines().contains("counter"));
}

TEST(StorageEngineTest, RepeatedExpireDoesNotGrowTheExpirationQueue) {
    StorageEngine storage;
    storage.set("session", Value("token"));
    const auto base = StorageEngine::Clock::now() + std::chrono::hours(1);

    for (int attempt = 0; attempt < 10'000; ++attempt) {
        ASSERT_TRUE(storage.expire_at("session", base + std::chrono::milliseconds(attempt)).applied);
    }

    EXPECT_LE(storage.expiration_queue_size(), 2U);
    EXPECT_EQ(storage.keys_with_deadlines().size(), 1U);
}

TEST(StorageEngineTest, ExpirationQueueStaysBoundedAcrossManyKeys) {
    StorageEngine storage;
    const auto deadline = StorageEngine::Clock::now() + std::chrono::hours(1);
    for (int key = 0; key < 100; ++key) storage.set(std::to_string(key), Value("v"));

    for (int round = 0; round < 50; ++round) {
        for (int key = 0; key < 100; ++key) {
            ASSERT_TRUE(storage.expire_at(std::to_string(key), deadline).applied);
        }
    }
    EXPECT_LE(storage.expiration_queue_size(), 200U);

    // Rebuilding keeps every tracked key reachable by the sweep.
    storage.prune_expired_batch(200, deadline);
    EXPECT_TRUE(storage.keys_with_deadlines().empty());
    EXPECT_FALSE(storage.exists("0"));
    EXPECT_FALSE(storage.exists("99"));
}
