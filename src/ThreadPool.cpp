#include "ThreadPool.hpp"

#include "Logger.hpp"

#include <exception>

namespace tcp {

ThreadPool::ThreadPool(std::size_t threadCount, std::size_t maxQueueSize) : maxQueueSize_(maxQueueSize) {
    if (threadCount == 0) {
        throw std::invalid_argument("ThreadPool: threadCount must be positive");
    }
    workers_.reserve(threadCount);
    try {
        for (std::size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back(&ThreadPool::workerLoop, this);
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

ThreadPool::~ThreadPool() { shutdown(); }

bool ThreadPool::trySubmit(Task task) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || (maxQueueSize_ != 0 && tasks_.size() >= maxQueueSize_)) {
            return false;
        }
        tasks_.push(std::move(task));
    }
    taskAvailable_.notify_one();
    return true;
}

void ThreadPool::shutdown() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    taskAvailable_.notify_all();

    std::lock_guard joinLock(joinMutex_);
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::size_t ThreadPool::pendingTasks() const {
    std::lock_guard lock(mutex_);
    return tasks_.size();
}

void ThreadPool::workerLoop() {
    for (;;) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            taskAvailable_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
            if (tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        try {
            task();
        } catch (const std::exception& e) {
            logError("Unhandled exception in worker thread: ", e.what());
        } catch (...) {
            logError("Unhandled unknown exception in worker thread");
        }
    }
}

}  // namespace tcp
