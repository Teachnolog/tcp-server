#include "LineBuffer.hpp"

#include <stdexcept>

namespace tcp {

LineBuffer::LineBuffer(std::size_t maxLineLength) : maxLineLength_(maxLineLength) {
    if (maxLineLength_ == 0) {
        throw std::invalid_argument("LineBuffer: maxLineLength must be positive");
    }
}

void LineBuffer::append(std::string_view data) { buffer_.append(data); }

std::optional<std::string> LineBuffer::popLine() {
    const std::size_t newline = buffer_.find('\n');
    if (newline == std::string::npos || newline > maxLineLength_) {
        return std::nullopt;
    }

    std::string line = buffer_.substr(0, newline);
    buffer_.erase(0, newline + 1);
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

bool LineBuffer::overflowed() const noexcept {
    const std::size_t newline = buffer_.find('\n');
    return newline == std::string::npos ? buffer_.size() > maxLineLength_ : newline > maxLineLength_;
}

}  // namespace tcp
