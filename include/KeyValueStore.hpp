#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace tcp {

class KeyValueStore {
public:
    void set(std::string key, std::string value);
    std::optional<std::string> get(const std::string& key) const;
    bool erase(const std::string& key);
    std::int64_t increment(const std::string& key, std::int64_t delta);
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};

}  // namespace tcp
