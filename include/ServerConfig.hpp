#pragma once

#include "Logger.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>

namespace tcp {

inline std::size_t defaultThreadCount() noexcept {
    const unsigned cores = std::thread::hardware_concurrency();
    return cores == 0 ? 4 : cores;
}

struct ServerConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 8080;
    std::size_t threadCount = defaultThreadCount();
    std::size_t maxQueuedConnections = 1024;  // 0 - без ограничения
    std::chrono::milliseconds idleTimeout{std::chrono::seconds(60)};  // 0 - без таймаута
    std::size_t maxLineLength = 4096;
    int backlog = 128;
    LogLevel logLevel = LogLevel::Info;
};

struct CommandLineOptions {
    ServerConfig config;
    bool showHelp = false;
};

CommandLineOptions parseCommandLine(int argc, const char* const* argv);
std::string usage(std::string_view programName);

}  // namespace tcp
