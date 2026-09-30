#include "Logger.hpp"

#include "StringUtils.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>

namespace tcp {
namespace {

std::atomic<LogLevel> gLevel{LogLevel::Info};
std::mutex gOutputMutex;

}  // namespace

std::string_view toString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Off:
            return "OFF";
    }
    return "UNKNOWN";
}

std::optional<LogLevel> parseLogLevel(std::string_view text) {
    const std::string name = toUpper(text);
    if (name == "DEBUG") return LogLevel::Debug;
    if (name == "INFO") return LogLevel::Info;
    if (name == "WARN" || name == "WARNING") return LogLevel::Warning;
    if (name == "ERROR") return LogLevel::Error;
    if (name == "OFF") return LogLevel::Off;
    return std::nullopt;
}

void Logger::setLevel(LogLevel level) noexcept { gLevel.store(level); }

LogLevel Logger::level() noexcept { return gLevel.load(); }

bool Logger::isEnabled(LogLevel level) noexcept {
    return level != LogLevel::Off && level >= Logger::level();
}

void Logger::write(LogLevel level, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm local{};
    localtime_r(&seconds, &local);

    std::ostringstream line;
    line << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
         << millis << " [" << toString(level) << "] [thread " << std::this_thread::get_id() << "] "
         << message << '\n';

    std::lock_guard lock(gOutputMutex);
    std::cerr << line.str();
}

}  // namespace tcp
