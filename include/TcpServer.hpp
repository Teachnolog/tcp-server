#pragma once

#include "CommandProcessor.hpp"
#include "KeyValueStore.hpp"
#include "ServerConfig.hpp"
#include "ServerStats.hpp"
#include "Socket.hpp"
#include "ThreadPool.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tcp {

class TcpServer {
public:
    explicit TcpServer(ServerConfig config,
                       std::shared_ptr<KeyValueStore> store = std::make_shared<KeyValueStore>());
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // блокирует поток до вызова stop()
    void run();
    void stop() noexcept;

    std::uint16_t port() const noexcept { return port_; }
    const ServerStats& stats() const noexcept { return *stats_; }

private:
    struct Connection;

    void dispatch(Socket client, std::string peer);
    void serve(const std::shared_ptr<Connection>& connection);
    void registerConnection(const std::shared_ptr<Connection>& connection);
    void unregisterConnection(std::uint64_t id);
    void shutdownAllConnections() noexcept;

    const ServerConfig config_;
    std::shared_ptr<KeyValueStore> store_;
    std::shared_ptr<ServerStats> stats_;
    CommandProcessor processor_;
    Socket listener_;
    const std::uint16_t port_;

    std::atomic<bool> stopRequested_{false};
    std::uint64_t nextConnectionId_ = 1;

    std::mutex connectionsMutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<Connection>> connections_;

    // пул должен уничтожаться первым, поэтому он последний
    ThreadPool pool_;
};

}  // namespace tcp
