#include "Logger.hpp"
#include "TcpClient.hpp"
#include "TcpServer.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using tcp::ServerConfig;
using tcp::TcpClient;
using tcp::TcpServer;

class TcpServerTest : public ::testing::Test {
protected:
    void SetUp() override { tcp::Logger::setLevel(tcp::LogLevel::Error); }

    void TearDown() override {
        stopServer();
        tcp::Logger::setLevel(tcp::LogLevel::Info);
    }

    static ServerConfig makeConfig(std::size_t threads = 4) {
        ServerConfig config;
        config.host = "127.0.0.1";
        config.port = 0;
        config.threadCount = threads;
        return config;
    }

    void startServer(const ServerConfig& config) {
        server = std::make_unique<TcpServer>(config);
        serverThread = std::thread([this] { server->run(); });
    }

    void stopServer() {
        if (server) {
            server->stop();
            serverThread.join();
            server.reset();
        }
    }

    TcpClient connect() const { return TcpClient("127.0.0.1", server->port()); }

    std::unique_ptr<TcpServer> server;
    std::thread serverThread;
};

TEST_F(TcpServerTest, RespondsToPing) {
    startServer(makeConfig());
    TcpClient client = connect();
    EXPECT_EQ(client.request("PING"), "PONG");
}

TEST_F(TcpServerTest, SharesStorageBetweenClients) {
    startServer(makeConfig());
    TcpClient writer = connect();
    TcpClient reader = connect();

    EXPECT_EQ(writer.request("SET greeting hello world"), "OK");
    EXPECT_EQ(reader.request("GET greeting"), "OK hello world");
    EXPECT_EQ(reader.request("DEL greeting"), "OK");
    EXPECT_EQ(writer.request("GET greeting"), "NOT_FOUND");
}

TEST_F(TcpServerTest, HandlesPipelinedAndFragmentedCommands) {
    startServer(makeConfig());
    TcpClient client = connect();

    client.sendRaw("PING\r\nECHO one\nEC");
    EXPECT_EQ(client.readLine(), "PONG");
    EXPECT_EQ(client.readLine(), "one");

    client.sendRaw("HO two\n");
    EXPECT_EQ(client.readLine(), "two");
}

TEST_F(TcpServerTest, ConcurrentClientsDoNotLoseUpdates) {
    constexpr int kClients = 16;
    constexpr int kRequestsPerClient = 200;
    startServer(makeConfig(/*threads=*/4));

    std::atomic<int> failures{0};
    std::vector<std::thread> clients;
    for (int i = 0; i < kClients; ++i) {
        clients.emplace_back([&] {
            try {
                TcpClient client = connect();
                for (int r = 0; r < kRequestsPerClient; ++r) {
                    if (client.request("INCR counter").rfind("OK ", 0) != 0) {
                        failures.fetch_add(1);
                    }
                }
            } catch (const std::exception&) {
                failures.fetch_add(1);
            }
        });
    }
    for (auto& client : clients) {
        client.join();
    }

    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(connect().request("GET counter"), "OK " + std::to_string(kClients * kRequestsPerClient));
}

TEST_F(TcpServerTest, RejectsClientsWhenQueueIsFull) {
    ServerConfig config = makeConfig(/*threads=*/1);
    config.maxQueuedConnections = 1;
    startServer(config);

    TcpClient active = connect();
    ASSERT_EQ(active.request("PING"), "PONG");

    TcpClient queued = connect();
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (active.request("STATS").find("connections_total=2 ") == std::string::npos) {
        ASSERT_LT(std::chrono::steady_clock::now(), deadline) << "server did not accept the second client";
        std::this_thread::sleep_for(10ms);
    }

    TcpClient rejected = connect();
    EXPECT_EQ(rejected.readLine(), "ERR server busy, try again later");
    EXPECT_FALSE(rejected.readLine().has_value());

    EXPECT_EQ(active.request("QUIT"), "BYE");
    EXPECT_EQ(queued.request("PING"), "PONG");
    EXPECT_EQ(server->stats().connectionsRejected.load(), 1u);
}

TEST_F(TcpServerTest, ClosesIdleConnections) {
    ServerConfig config = makeConfig();
    config.idleTimeout = 200ms;
    startServer(config);

    TcpClient client = connect();
    EXPECT_EQ(client.readLine(), "ERR idle timeout, closing connection");
    EXPECT_FALSE(client.readLine().has_value());
}

TEST_F(TcpServerTest, RejectsTooLongLines) {
    ServerConfig config = makeConfig();
    config.maxLineLength = 16;
    startServer(config);

    TcpClient client = connect();
    client.sendRaw(std::string(100, 'x'));
    EXPECT_EQ(client.readLine(), "ERR line too long (limit is 16 bytes)");
    EXPECT_FALSE(client.readLine().has_value());
}

TEST_F(TcpServerTest, QuitClosesConnection) {
    startServer(makeConfig());
    TcpClient client = connect();
    EXPECT_EQ(client.request("QUIT"), "BYE");
    EXPECT_FALSE(client.readLine().has_value());
}

TEST_F(TcpServerTest, StopDisconnectsActiveClientsPromptly) {
    startServer(makeConfig(/*threads=*/2));
    TcpClient first = connect();
    TcpClient second = connect();
    ASSERT_EQ(first.request("PING"), "PONG");
    ASSERT_EQ(second.request("PING"), "PONG");

    const auto begin = std::chrono::steady_clock::now();
    stopServer();
    EXPECT_LT(std::chrono::steady_clock::now() - begin, 2s);

    EXPECT_FALSE(first.readLine().has_value());
    EXPECT_FALSE(second.readLine().has_value());
}

TEST_F(TcpServerTest, FailsToBindBusyPort) {
    startServer(makeConfig());
    ServerConfig config = makeConfig();
    config.port = server->port();
    EXPECT_THROW(TcpServer{config}, std::system_error);
}
