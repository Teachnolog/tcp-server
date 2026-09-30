#include "ServerConfig.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

using tcp::parseCommandLine;

namespace {

tcp::CommandLineOptions parse(std::vector<const char*> arguments) {
    arguments.insert(arguments.begin(), "server_app");
    return parseCommandLine(static_cast<int>(arguments.size()), arguments.data());
}

}  // namespace

TEST(ServerConfig, UsesDefaultsWithoutArguments) {
    const auto options = parse({});
    EXPECT_FALSE(options.showHelp);
    EXPECT_EQ(options.config.port, 8080);
    EXPECT_EQ(options.config.host, "0.0.0.0");
    EXPECT_GE(options.config.threadCount, 1u);
}

TEST(ServerConfig, ParsesAllOptions) {
    const auto options = parse({"--host", "127.0.0.1", "--port=9000", "--threads", "3", "--queue", "10",
                                "--idle-timeout", "5", "--max-line", "128", "--log-level", "debug"});
    const auto& config = options.config;
    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 9000);
    EXPECT_EQ(config.threadCount, 3u);
    EXPECT_EQ(config.maxQueuedConnections, 10u);
    EXPECT_EQ(config.idleTimeout, std::chrono::seconds(5));
    EXPECT_EQ(config.maxLineLength, 128u);
    EXPECT_EQ(config.logLevel, tcp::LogLevel::Debug);
}

TEST(ServerConfig, RecognizesHelp) { EXPECT_TRUE(parse({"--help"}).showHelp); }

TEST(ServerConfig, RejectsInvalidInput) {
    EXPECT_THROW(parse({"--port", "70000"}), std::invalid_argument);
    EXPECT_THROW(parse({"--port", "80a"}), std::invalid_argument);
    EXPECT_THROW(parse({"--threads", "0"}), std::invalid_argument);
    EXPECT_THROW(parse({"--threads"}), std::invalid_argument);
    EXPECT_THROW(parse({"--log-level", "loud"}), std::invalid_argument);
    EXPECT_THROW(parse({"--unknown"}), std::invalid_argument);
}
