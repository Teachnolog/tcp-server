#include "CommandProcessor.hpp"

#include <gtest/gtest.h>

#include <memory>

using tcp::CommandProcessor;
using tcp::KeyValueStore;
using tcp::ServerStats;

class CommandProcessorTest : public ::testing::Test {
protected:
    std::string run(std::string_view line) { return processor.execute(line).text; }

    std::shared_ptr<KeyValueStore> store = std::make_shared<KeyValueStore>();
    std::shared_ptr<ServerStats> stats = std::make_shared<ServerStats>();
    CommandProcessor processor{store, stats};
};

TEST_F(CommandProcessorTest, Ping) {
    EXPECT_EQ(run("PING"), "PONG");
    EXPECT_EQ(run("  ping  "), "PONG");
    EXPECT_EQ(run("PING extra"), "ERR usage: PING");
}

TEST_F(CommandProcessorTest, EchoKeepsInnerSpaces) {
    EXPECT_EQ(run("ECHO hello   world"), "hello   world");
    EXPECT_EQ(run("ECHO"), "ERR usage: ECHO <text>");
}

TEST_F(CommandProcessorTest, SetGetDel) {
    EXPECT_EQ(run("GET city"), "NOT_FOUND");
    EXPECT_EQ(run("SET city Saint Petersburg"), "OK");
    EXPECT_EQ(run("get city"), "OK Saint Petersburg");
    EXPECT_EQ(run("SIZE"), "OK 1");
    EXPECT_EQ(run("DEL city"), "OK");
    EXPECT_EQ(run("DEL city"), "NOT_FOUND");
    EXPECT_EQ(run("SIZE"), "OK 0");
}

TEST_F(CommandProcessorTest, ValidatesArity) {
    EXPECT_EQ(run("SET key"), "ERR usage: SET <key> <value>");
    EXPECT_EQ(run("GET"), "ERR usage: GET <key>");
    EXPECT_EQ(run("GET a b"), "ERR usage: GET <key>");
    EXPECT_EQ(run("DEL"), "ERR usage: DEL <key>");
    EXPECT_EQ(run("INCR a 1 2"), "ERR usage: INCR <key> [delta]");
}

TEST_F(CommandProcessorTest, Incr) {
    EXPECT_EQ(run("INCR visits"), "OK 1");
    EXPECT_EQ(run("INCR visits 10"), "OK 11");
    EXPECT_EQ(run("INCR visits -1"), "OK 10");
    EXPECT_EQ(run("INCR visits abc"), "ERR delta must be a 64-bit integer");

    run("SET name bob");
    EXPECT_EQ(run("INCR name"), "ERR value is not a 64-bit integer");
}

TEST_F(CommandProcessorTest, UnknownCommand) {
    EXPECT_EQ(run("FLY away"), "ERR unknown command 'FLY', try HELP");
    EXPECT_EQ(run("   "), "ERR empty command");
}

TEST_F(CommandProcessorTest, QuitClosesConnection) {
    const auto response = processor.execute("QUIT");
    EXPECT_EQ(response.text, "BYE");
    EXPECT_TRUE(response.closeConnection);
    EXPECT_FALSE(processor.execute("PING").closeConnection);
}

TEST_F(CommandProcessorTest, StatsReportsCounters) {
    stats->connectionsTotal = 3;
    stats->commandsProcessed = 42;
    run("SET a 1");

    const std::string response = run("STATS");
    EXPECT_EQ(response.rfind("OK ", 0), 0u);
    EXPECT_NE(response.find("connections_total=3 "), std::string::npos);
    EXPECT_NE(response.find("commands_processed=42 "), std::string::npos);
    EXPECT_NE(response.find("keys=1 "), std::string::npos);
}

TEST_F(CommandProcessorTest, HelpListsCommands) { EXPECT_NE(run("HELP").find("INCR"), std::string::npos); }

TEST(CommandProcessor, RejectsNullDependencies) {
    EXPECT_THROW(CommandProcessor(nullptr, std::make_shared<ServerStats>()), std::invalid_argument);
}
