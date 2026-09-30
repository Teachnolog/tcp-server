#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace tcp {

// TCP - это поток байт, поэтому команды собираем в буфере до '\n'
class LineBuffer {
public:
    explicit LineBuffer(std::size_t maxLineLength);

    void append(std::string_view data);
    std::optional<std::string> popLine();
    bool overflowed() const noexcept;

    std::size_t bufferedBytes() const noexcept { return buffer_.size(); }

private:
    std::string buffer_;
    std::size_t maxLineLength_;
};

}  // namespace tcp
