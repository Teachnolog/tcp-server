#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace tcp {

enum class IoStatus { Ok, Closed, Timeout, Error };

struct ReceiveResult {
    IoStatus status;
    std::size_t bytes;
};

class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(int fd) noexcept;
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    static Socket listenOn(const std::string& host, std::uint16_t port, int backlog);
    static Socket connectTo(const std::string& host, std::uint16_t port);

    // возвращает невалидный сокет, если подключений нет (EAGAIN, EINTR)
    Socket accept(std::string* peerAddress = nullptr);

    ReceiveResult receive(char* buffer, std::size_t size);
    bool sendAll(std::string_view data);

    void setNonBlocking(bool enabled);
    void setTimeouts(std::chrono::milliseconds timeout);
    void setNoDelay(bool enabled);

    void shutdown() noexcept;
    void close() noexcept;

    std::uint16_t localPort() const;
    int fd() const noexcept { return fd_; }
    bool isValid() const noexcept { return fd_ >= 0; }
    explicit operator bool() const noexcept { return isValid(); }

private:
    int fd_ = -1;
};

}  // namespace tcp
