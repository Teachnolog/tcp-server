#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace tcp {

class ThreadPool {
public:
    using Task = std::function<void()>;

    // maxQueueSize == 0 - очередь без ограничения
    explicit ThreadPool(std::size_t threadCount, std::size_t maxQueueSize = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename F, typename... Args>
    auto submit(F&& function, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

    [[nodiscard]] bool trySubmit(Task task);

    void shutdown();

    std::size_t threadCount() const noexcept { return workers_.size(); }
    std::size_t pendingTasks() const;

private:
    void workerLoop();

    const std::size_t maxQueueSize_;
    std::vector<std::thread> workers_;

    mutable std::mutex mutex_;
    std::condition_variable taskAvailable_;
    std::queue<Task> tasks_;
    bool stopping_ = false;

    std::mutex joinMutex_;
};

template <typename F, typename... Args>
auto ThreadPool::submit(F&& function, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using Result = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    // packaged_task нельзя копировать, а std::function требует копирования, поэтому shared_ptr
    auto task = std::make_shared<std::packaged_task<Result()>>(
        [fn = std::forward<F>(function),
         boundArgs = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Result {
            return std::apply(std::move(fn), std::move(boundArgs));
        });

    std::future<Result> result = task->get_future();
    if (!trySubmit([task] { (*task)(); })) {
        throw std::runtime_error("ThreadPool: task rejected (pool is stopped or queue is full)");
    }
    return result;
}

}  // namespace tcp
