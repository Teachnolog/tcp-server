#pragma once

#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace tcp {

enum class LogLevel { Debug = 0, Info, Warning, Error, Off };

std::string_view toString(LogLevel level) noexcept;
std::optional<LogLevel> parseLogLevel(std::string_view text);

class Logger {
public:
    static void setLevel(LogLevel level) noexcept;
    static LogLevel level() noexcept;
    static bool isEnabled(LogLevel level) noexcept;
    static void write(LogLevel level, std::string_view message);

    template <typename... Args>
    static void log(LogLevel level, Args&&... args) {
        if (!isEnabled(level)) {
            return;
        }
        std::ostringstream message;
        (message << ... << std::forward<Args>(args));
        write(level, message.str());
    }
};

template <typename... Args>
void logDebug(Args&&... args) {
    Logger::log(LogLevel::Debug, std::forward<Args>(args)...);
}

template <typename... Args>
void logInfo(Args&&... args) {
    Logger::log(LogLevel::Info, std::forward<Args>(args)...);
}

template <typename... Args>
void logWarning(Args&&... args) {
    Logger::log(LogLevel::Warning, std::forward<Args>(args)...);
}

template <typename... Args>
void logError(Args&&... args) {
    Logger::log(LogLevel::Error, std::forward<Args>(args)...);
}

}  // namespace tcp
