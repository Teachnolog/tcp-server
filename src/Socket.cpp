#include "Socket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace tcp {
namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;  // на macOS нет MSG_NOSIGNAL, SIGPIPE игнорируется в main
#endif

[[noreturn]] void throwSystemError(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

sockaddr_in resolveIPv4(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* found = nullptr;
    const int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &found);
    if (rc != 0) {
        throw std::runtime_error("cannot resolve '" + host + "': " + ::gai_strerror(rc));
    }
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> guard(found, &::freeaddrinfo);

    sockaddr_in address{};
    std::memcpy(&address, found->ai_addr, sizeof(address));
    address.sin_port = htons(port);
    return address;
}

Socket createTcpSocket() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throwSystemError("socket");
    }
    return Socket(fd);
}

}  // namespace

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket::~Socket() { close(); }

Socket::Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

Socket Socket::listenOn(const std::string& host, std::uint16_t port, int backlog) {
    const sockaddr_in address = resolveIPv4(host, port);
    Socket listener = createTcpSocket();

    const int reuse = 1;
    if (::setsockopt(listener.fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        throwSystemError("setsockopt(SO_REUSEADDR)");
    }
    if (::bind(listener.fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throwSystemError("bind to " + host + ":" + std::to_string(port));
    }
    if (::listen(listener.fd_, backlog) < 0) {
        throwSystemError("listen");
    }
    return listener;
}

Socket Socket::connectTo(const std::string& host, std::uint16_t port) {
    const sockaddr_in address = resolveIPv4(host, port);
    Socket connection = createTcpSocket();
    if (::connect(connection.fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throwSystemError("connect to " + host + ":" + std::to_string(port));
    }
    return connection;
}

Socket Socket::accept(std::string* peerAddress) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    const int clientFd = ::accept(fd_, reinterpret_cast<sockaddr*>(&address), &length);
    if (clientFd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED) {
            return Socket{};
        }
        throwSystemError("accept");
    }

    Socket client(clientFd);
    // на BSD/macOS O_NONBLOCK наследуется от слушающего сокета
    client.setNonBlocking(false);

    if (peerAddress != nullptr) {
        char text[INET_ADDRSTRLEN] = {};
        ::inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text));
        *peerAddress = std::string(text) + ':' + std::to_string(ntohs(address.sin_port));
    }
    return client;
}

ReceiveResult Socket::receive(char* buffer, std::size_t size) {
    for (;;) {
        const ssize_t received = ::recv(fd_, buffer, size, 0);
        if (received > 0) {
            return {IoStatus::Ok, static_cast<std::size_t>(received)};
        }
        if (received == 0) {
            return {IoStatus::Closed, 0};
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return {IoStatus::Timeout, 0};
        }
        return {IoStatus::Error, 0};
    }
}

bool Socket::sendAll(std::string_view data) {
    while (!data.empty()) {
        const ssize_t sent = ::send(fd_, data.data(), data.size(), kSendFlags);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(sent));
    }
    return true;
}

void Socket::setNonBlocking(bool enabled) {
    const int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        throwSystemError("fcntl(F_GETFL)");
    }
    const int updated = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(fd_, F_SETFL, updated) < 0) {
        throwSystemError("fcntl(F_SETFL)");
    }
}

void Socket::setTimeouts(std::chrono::milliseconds timeout) {
    timeval value{};
    value.tv_sec = static_cast<decltype(value.tv_sec)>(timeout.count() / 1000);
    value.tv_usec = static_cast<decltype(value.tv_usec)>((timeout.count() % 1000) * 1000);
    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) < 0) {
        throwSystemError("setsockopt(SO_RCVTIMEO)");
    }
    if (::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value)) < 0) {
        throwSystemError("setsockopt(SO_SNDTIMEO)");
    }
}

void Socket::setNoDelay(bool enabled) {
    const int value = enabled ? 1 : 0;
    if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) < 0) {
        throwSystemError("setsockopt(TCP_NODELAY)");
    }
}

void Socket::shutdown() noexcept {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
    }
}

void Socket::close() noexcept {
    if (fd_ >= 0) {
        ::close(std::exchange(fd_, -1));
    }
}

std::uint16_t Socket::localPort() const {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
        throwSystemError("getsockname");
    }
    return ntohs(address.sin_port);
}

}  // namespace tcp
