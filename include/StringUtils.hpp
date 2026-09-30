#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace tcp {

inline bool isSpace(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

inline std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

// "SET key some value" -> {"SET", "key some value"}
inline std::pair<std::string_view, std::string_view> splitToken(std::string_view text) noexcept {
    text = trim(text);
    std::size_t end = 0;
    while (end < text.size() && !isSpace(text[end])) {
        ++end;
    }
    return {text.substr(0, end), trim(text.substr(end))};
}

inline std::string toUpper(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

template <typename Number>
std::optional<Number> parseNumber(std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    Number value{};
    const char* const end = text.data() + text.size();
    const auto [parsedEnd, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc{} || parsedEnd != end) {
        return std::nullopt;
    }
    return value;
}

}  // namespace tcp
