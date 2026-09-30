#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace tcp {

struct ServerStats {
    std::atomic<std::uint64_t> connectionsTotal{0};
    std::atomic<std::uint64_t> connectionsActive{0};
    std::atomic<std::uint64_t> connectionsRejected{0};
    std::atomic<std::uint64_t> commandsProcessed{0};
    const std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
};

}  // namespace tcp
