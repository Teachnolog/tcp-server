#include "KeyValueStore.hpp"

#include "StringUtils.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace tcp {

void KeyValueStore::set(std::string key, std::string value) {
    std::lock_guard lock(mutex_);
    data_.insert_or_assign(std::move(key), std::move(value));
}

std::optional<std::string> KeyValueStore::get(const std::string& key) const {
    std::lock_guard lock(mutex_);
    const auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool KeyValueStore::erase(const std::string& key) {
    std::lock_guard lock(mutex_);
    return data_.erase(key) > 0;
}

std::int64_t KeyValueStore::increment(const std::string& key, std::int64_t delta) {
    constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
    constexpr auto kMin = std::numeric_limits<std::int64_t>::min();

    std::lock_guard lock(mutex_);
    const auto it = data_.find(key);

    std::int64_t current = 0;
    if (it != data_.end()) {
        const auto parsed = parseNumber<std::int64_t>(it->second);
        if (!parsed) {
            throw std::invalid_argument("value is not a 64-bit integer");
        }
        current = *parsed;
    }

    if ((delta > 0 && current > kMax - delta) || (delta < 0 && current < kMin - delta)) {
        throw std::overflow_error("increment would overflow");
    }

    const std::int64_t updated = current + delta;
    if (it != data_.end()) {
        it->second = std::to_string(updated);
    } else {
        data_.emplace(key, std::to_string(updated));
    }
    return updated;
}

std::size_t KeyValueStore::size() const {
    std::lock_guard lock(mutex_);
    return data_.size();
}

}  // namespace tcp
