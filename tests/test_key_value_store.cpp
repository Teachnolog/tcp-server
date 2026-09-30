#include "KeyValueStore.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

using tcp::KeyValueStore;

TEST(KeyValueStore, SetGetErase) {
    KeyValueStore store;
    EXPECT_FALSE(store.get("name").has_value());

    store.set("name", "tcp-server");
    EXPECT_EQ(store.get("name"), "tcp-server");
    EXPECT_EQ(store.size(), 1u);

    store.set("name", "updated");
    EXPECT_EQ(store.get("name"), "updated");

    EXPECT_TRUE(store.erase("name"));
    EXPECT_FALSE(store.erase("name"));
    EXPECT_EQ(store.size(), 0u);
}

TEST(KeyValueStore, IncrementTreatsMissingKeyAsZero) {
    KeyValueStore store;
    EXPECT_EQ(store.increment("counter", 1), 1);
    EXPECT_EQ(store.increment("counter", 10), 11);
    EXPECT_EQ(store.increment("counter", -20), -9);
    EXPECT_EQ(store.get("counter"), "-9");
}

TEST(KeyValueStore, IncrementRejectsNonIntegerValue) {
    KeyValueStore store;
    store.set("text", "hello");
    EXPECT_THROW(store.increment("text", 1), std::invalid_argument);
    EXPECT_EQ(store.get("text"), "hello");
}

TEST(KeyValueStore, IncrementDetectsOverflow) {
    KeyValueStore store;
    store.set("big", std::to_string(std::numeric_limits<std::int64_t>::max()));
    EXPECT_THROW(store.increment("big", 1), std::overflow_error);

    store.set("small", std::to_string(std::numeric_limits<std::int64_t>::min()));
    EXPECT_THROW(store.increment("small", -1), std::overflow_error);
}

TEST(KeyValueStore, ConcurrentIncrementsAreNotLost) {
    constexpr int kThreads = 8;
    constexpr int kIncrementsPerThread = 10'000;

    KeyValueStore store;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&store] {
            for (int i = 0; i < kIncrementsPerThread; ++i) {
                store.increment("counter", 1);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(store.get("counter"), std::to_string(kThreads * kIncrementsPerThread));
}
