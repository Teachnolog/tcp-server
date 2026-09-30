#include "Logger.hpp"
#include "ThreadPool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;
using tcp::ThreadPool;

TEST(ThreadPool, ReturnsResultsThroughFutures) {
    ThreadPool pool(4);
    std::vector<std::future<int>> results;
    for (int i = 0; i < 100; ++i) {
        results.push_back(pool.submit([i] { return i * i; }));
    }
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(results[static_cast<std::size_t>(i)].get(), i * i);
    }
}

TEST(ThreadPool, ForwardsArguments) {
    ThreadPool pool(2);
    auto result = pool.submit([](int number, const std::string& text) { return text + std::to_string(number); },
                              42, std::string("answer="));
    EXPECT_EQ(result.get(), "answer=42");
}

TEST(ThreadPool, PropagatesExceptionsThroughFuture) {
    ThreadPool pool(1);
    auto result = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    EXPECT_THROW(result.get(), std::runtime_error);
}

TEST(ThreadPool, RunsTasksInParallel) {
    constexpr std::size_t kThreads = 4;
    ThreadPool pool(kThreads);

    std::mutex mutex;
    std::condition_variable allArrived;
    std::size_t arrived = 0;

    std::vector<std::future<bool>> results;
    for (std::size_t i = 0; i < kThreads; ++i) {
        results.push_back(pool.submit([&] {
            std::unique_lock lock(mutex);
            ++arrived;
            allArrived.notify_all();
            return allArrived.wait_for(lock, 5s, [&] { return arrived == kThreads; });
        }));
    }
    for (auto& result : results) {
        EXPECT_TRUE(result.get());
    }
}

TEST(ThreadPool, ShutdownFinishesQueuedTasks) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 1000; ++i) {
            pool.submit([&counter] { counter.fetch_add(1); });
        }
        pool.shutdown();
    }
    EXPECT_EQ(counter.load(), 1000);
}

TEST(ThreadPool, RejectsTasksAfterShutdown) {
    ThreadPool pool(1);
    pool.shutdown();
    pool.shutdown();
    EXPECT_THROW(pool.submit([] {}), std::runtime_error);
    EXPECT_FALSE(pool.trySubmit([] {}));
}

TEST(ThreadPool, BoundedQueueRejectsOverflow) {
    ThreadPool pool(1, /*maxQueueSize=*/1);

    std::promise<void> started;
    std::future<void> startedFuture = started.get_future();
    std::promise<void> release;
    std::shared_future<void> releaseFuture = release.get_future().share();

    ASSERT_TRUE(pool.trySubmit([&started, releaseFuture] {
        started.set_value();
        releaseFuture.wait();
    }));
    startedFuture.wait();

    EXPECT_TRUE(pool.trySubmit([] {}));
    EXPECT_FALSE(pool.trySubmit([] {}));
    EXPECT_EQ(pool.pendingTasks(), 1u);

    release.set_value();
}

TEST(ThreadPool, WorkerSurvivesThrowingTask) {
    tcp::Logger::setLevel(tcp::LogLevel::Off);
    ThreadPool pool(1);
    ASSERT_TRUE(pool.trySubmit([] { throw std::runtime_error("unhandled"); }));
    EXPECT_EQ(pool.submit([] { return 7; }).get(), 7);
    tcp::Logger::setLevel(tcp::LogLevel::Info);
}

TEST(ThreadPool, RejectsZeroThreads) { EXPECT_THROW(ThreadPool(0), std::invalid_argument); }
