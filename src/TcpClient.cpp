#include "TcpClient.hpp"

#include <array>
#include <stdexcept>

namespace tcp {
namespace {

constexpr std::size_t kMaxResponseLength = 1 << 20;

}  // namespace

TcpClient::TcpClient(const std::string& host, std::uint16_t port, std::chrono::milliseconds timeout)
    : socket_(Socket::connectTo(host, port)), buffer_(kMaxResponseLength) {
    socket_.setTimeouts(timeout);
    socket_.setNoDelay(true);
}

void TcpClient::sendRaw(std::string_view data) {
    if (!socket_.sendAll(data)) {
        throw std::runtime_error("send failed: connection lost");
    }
}

void TcpClient::sendLine(std::string_view line) {
    std::string message(line);
    message += '\n';
    sendRaw(message);
}

std::optional<std::string> TcpClient::readLine() {
    std::array<char, 4096> chunk{};
    for (;;) {
        if (auto line = buffer_.popLine()) {
            return line;
        }
        const ReceiveResult received = socket_.receive(chunk.data(), chunk.size());
        switch (received.status) {
            case IoStatus::Ok:
                buffer_.append(std::string_view(chunk.data(), received.bytes));
                break;
            case IoStatus::Timeout:
                throw std::runtime_error("timed out waiting for server response");
            case IoStatus::Closed:
            case IoStatus::Error:
                return std::nullopt;
        }
    }
}

std::string TcpClient::request(std::string_view line) {
    sendLine(line);
    auto response = readLine();
    if (!response) {
        throw std::runtime_error("connection closed by server");
    }
    return *response;
}

}  // namespace tcp
