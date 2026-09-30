#include "TcpServer.hpp"

#include "LineBuffer.hpp"
#include "Logger.hpp"
#include "StringUtils.hpp"

#include <poll.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

namespace tcp {
namespace {

constexpr int kPollIntervalMs = 100;
constexpr std::size_t kReadChunkSize = 4096;

}  // namespace

struct TcpServer::Connection {
    Connection(std::uint64_t connectionId, std::string peerAddress, Socket clientSocket)
        : id(connectionId), peer(std::move(peerAddress)), socket(std::move(clientSocket)) {}

    const std::uint64_t id;
    const std::string peer;
    Socket socket;
};

TcpServer::TcpServer(ServerConfig config, std::shared_ptr<KeyValueStore> store)
    : config_(std::move(config)),
      store_(std::move(store)),
      stats_(std::make_shared<ServerStats>()),
      processor_(store_, stats_),
      listener_(Socket::listenOn(config_.host, config_.port, config_.backlog)),
      port_(listener_.localPort()),
      pool_(config_.threadCount, config_.maxQueuedConnections) {
    listener_.setNonBlocking(true);
}

TcpServer::~TcpServer() {
    stop();
    pool_.shutdown();
}

void TcpServer::run() {
    logInfo("Server listening on ", config_.host, ':', port_, " (worker threads: ", pool_.threadCount(),
            ", queue limit: ", config_.maxQueuedConnections, ")");

    pollfd listenerPoll{};
    listenerPoll.fd = listener_.fd();
    listenerPoll.events = POLLIN;

    while (!stopRequested_.load()) {
        listenerPoll.revents = 0;
        const int ready = ::poll(&listenerPoll, 1, kPollIntervalMs);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            logError("poll() failed: ", std::generic_category().message(errno));
            break;
        }
        if (ready == 0) {
            continue;
        }

        try {
            std::string peer;
            Socket client = listener_.accept(&peer);
            if (client) {
                dispatch(std::move(client), std::move(peer));
            }
        } catch (const std::exception& e) {
            logError("Failed to accept connection: ", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
        }
    }

    logInfo("Shutting down: waiting for worker threads to finish");
    shutdownAllConnections();
    pool_.shutdown();
    logInfo("Server stopped. Connections served: ", stats_->connectionsTotal.load(),
            ", commands processed: ", stats_->commandsProcessed.load());
}

void TcpServer::stop() noexcept {
    if (stopRequested_.exchange(true)) {
        return;
    }
    shutdownAllConnections();
}

void TcpServer::dispatch(Socket client, std::string peer) {
    auto connection = std::make_shared<Connection>(nextConnectionId_++, std::move(peer), std::move(client));
    try {
        connection->socket.setTimeouts(config_.idleTimeout);
        connection->socket.setNoDelay(true);
    } catch (const std::exception& e) {
        logWarning("Cannot configure connection from ", connection->peer, ": ", e.what());
        return;
    }

    stats_->connectionsTotal++;
    registerConnection(connection);

    if (!pool_.trySubmit([this, connection] { serve(connection); })) {
        stats_->connectionsRejected++;
        logWarning("Rejecting client #", connection->id, " (", connection->peer, "): server is busy");
        connection->socket.sendAll("ERR server busy, try again later\n");
        unregisterConnection(connection->id);
    }
}

void TcpServer::serve(const std::shared_ptr<Connection>& connection) {
    logInfo("Client #", connection->id, " connected from ", connection->peer);

    LineBuffer lines(config_.maxLineLength);
    std::array<char, kReadChunkSize> chunk{};
    std::string reply;
    bool keepOpen = true;

    try {
        while (keepOpen && !stopRequested_.load()) {
            const ReceiveResult received = connection->socket.receive(chunk.data(), chunk.size());
            if (received.status == IoStatus::Timeout) {
                logInfo("Client #", connection->id, " idle for too long, closing");
                connection->socket.sendAll("ERR idle timeout, closing connection\n");
                break;
            }
            if (received.status != IoStatus::Ok) {
                break;
            }

            lines.append(std::string_view(chunk.data(), received.bytes));

            // за один recv может прийти несколько команд - отвечаем на все одним send
            reply.clear();
            while (auto line = lines.popLine()) {
                if (trim(*line).empty()) {
                    continue;
                }
                const Response response = processor_.execute(*line);
                stats_->commandsProcessed++;
                reply += response.text;
                reply += '\n';
                if (response.closeConnection) {
                    keepOpen = false;
                    break;
                }
            }
            if (keepOpen && lines.overflowed()) {
                reply += "ERR line too long (limit is " + std::to_string(config_.maxLineLength) + " bytes)\n";
                keepOpen = false;
            }
            if (!reply.empty() && !connection->socket.sendAll(reply)) {
                break;
            }
        }
    } catch (const std::exception& e) {
        logError("Client #", connection->id, ": ", e.what());
    } catch (...) {
        logError("Client #", connection->id, ": unknown error");
    }

    unregisterConnection(connection->id);
    logInfo("Client #", connection->id, " disconnected");
}

void TcpServer::registerConnection(const std::shared_ptr<Connection>& connection) {
    std::lock_guard lock(connectionsMutex_);
    connections_.emplace(connection->id, connection);
    stats_->connectionsActive++;
}

void TcpServer::unregisterConnection(std::uint64_t id) {
    std::lock_guard lock(connectionsMutex_);
    if (connections_.erase(id) > 0) {
        stats_->connectionsActive--;
    }
}

void TcpServer::shutdownAllConnections() noexcept {
    // пока соединение в map, его сокет не закрыт, так что fd тут всегда валидный
    std::lock_guard lock(connectionsMutex_);
    for (auto& entry : connections_) {
        entry.second->socket.shutdown();
    }
}

}  // namespace tcp
