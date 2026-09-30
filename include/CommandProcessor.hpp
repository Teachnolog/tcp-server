#pragma once

#include "KeyValueStore.hpp"
#include "ServerStats.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tcp {

struct Response {
    std::string text;
    bool closeConnection = false;
};

class CommandProcessor {
public:
    CommandProcessor(std::shared_ptr<KeyValueStore> store, std::shared_ptr<const ServerStats> stats);

    Response execute(std::string_view line) const;

private:
    using Handler = Response (CommandProcessor::*)(std::string_view arguments) const;
    static const std::unordered_map<std::string, Handler>& handlers();

    Response handlePing(std::string_view arguments) const;
    Response handleEcho(std::string_view arguments) const;
    Response handleSet(std::string_view arguments) const;
    Response handleGet(std::string_view arguments) const;
    Response handleDel(std::string_view arguments) const;
    Response handleIncr(std::string_view arguments) const;
    Response handleSize(std::string_view arguments) const;
    Response handleStats(std::string_view arguments) const;
    Response handleHelp(std::string_view arguments) const;
    Response handleQuit(std::string_view arguments) const;

    std::shared_ptr<KeyValueStore> store_;
    std::shared_ptr<const ServerStats> stats_;
};

}  // namespace tcp
