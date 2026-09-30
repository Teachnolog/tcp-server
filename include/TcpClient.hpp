#pragma once

#include "LineBuffer.hpp"
#include "Socket.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tcp {

class TcpClient {
public:
    TcpClient(const std::string& host, std::uint16_t port,
              std::chrono::milliseconds timeout = std::chrono::seconds(5));

    void sendRaw(std::string_view data);
    void sendLine(std::string_view line);

    // nullopt - сервер закрыл соединение
    std::optional<std::string> readLine();

    std::string request(std::string_view line);

private:
    Socket socket_;
    LineBuffer buffer_;
};

}  // namespace tcp
