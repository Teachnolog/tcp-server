#include "CommandProcessor.hpp"

#include "StringUtils.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace tcp {
namespace {

Response ok(std::string_view payload = {}) {
    return {payload.empty() ? std::string("OK") : "OK " + std::string(payload)};
}

Response error(std::string_view message) { return {"ERR " + std::string(message)}; }

Response usage(std::string_view syntax) { return error("usage: " + std::string(syntax)); }

Response notFound() { return {"NOT_FOUND"}; }

}  // namespace

CommandProcessor::CommandProcessor(std::shared_ptr<KeyValueStore> store,
                                   std::shared_ptr<const ServerStats> stats)
    : store_(std::move(store)), stats_(std::move(stats)) {
    if (!store_ || !stats_) {
        throw std::invalid_argument("CommandProcessor: store and stats must not be null");
    }
}

const std::unordered_map<std::string, CommandProcessor::Handler>& CommandProcessor::handlers() {
    static const std::unordered_map<std::string, Handler> table = {
        {"PING", &CommandProcessor::handlePing},   {"ECHO", &CommandProcessor::handleEcho},
        {"SET", &CommandProcessor::handleSet},     {"GET", &CommandProcessor::handleGet},
        {"DEL", &CommandProcessor::handleDel},     {"INCR", &CommandProcessor::handleIncr},
        {"SIZE", &CommandProcessor::handleSize},   {"STATS", &CommandProcessor::handleStats},
        {"HELP", &CommandProcessor::handleHelp},   {"QUIT", &CommandProcessor::handleQuit},
    };
    return table;
}

Response CommandProcessor::execute(std::string_view line) const {
    const auto [name, arguments] = splitToken(line);
    if (name.empty()) {
        return error("empty command");
    }

    const auto it = handlers().find(toUpper(name));
    if (it == handlers().end()) {
        return error("unknown command '" + std::string(name) + "', try HELP");
    }

    try {
        return (this->*(it->second))(arguments);
    } catch (const std::exception& e) {
        return error(e.what());
    }
}

Response CommandProcessor::handlePing(std::string_view arguments) const {
    if (!arguments.empty()) {
        return usage("PING");
    }
    return {"PONG"};
}

Response CommandProcessor::handleEcho(std::string_view arguments) const {
    if (arguments.empty()) {
        return usage("ECHO <text>");
    }
    return {std::string(arguments)};
}

Response CommandProcessor::handleSet(std::string_view arguments) const {
    const auto [key, value] = splitToken(arguments);
    if (key.empty() || value.empty()) {
        return usage("SET <key> <value>");
    }
    store_->set(std::string(key), std::string(value));
    return ok();
}

Response CommandProcessor::handleGet(std::string_view arguments) const {
    const auto [key, rest] = splitToken(arguments);
    if (key.empty() || !rest.empty()) {
        return usage("GET <key>");
    }
    if (const auto value = store_->get(std::string(key))) {
        return ok(*value);
    }
    return notFound();
}

Response CommandProcessor::handleDel(std::string_view arguments) const {
    const auto [key, rest] = splitToken(arguments);
    if (key.empty() || !rest.empty()) {
        return usage("DEL <key>");
    }
    return store_->erase(std::string(key)) ? ok() : notFound();
}

Response CommandProcessor::handleIncr(std::string_view arguments) const {
    const auto [key, rest] = splitToken(arguments);
    const auto [deltaText, extra] = splitToken(rest);
    if (key.empty() || !extra.empty()) {
        return usage("INCR <key> [delta]");
    }

    std::int64_t delta = 1;
    if (!deltaText.empty()) {
        const auto parsed = parseNumber<std::int64_t>(deltaText);
        if (!parsed) {
            return error("delta must be a 64-bit integer");
        }
        delta = *parsed;
    }
    return ok(std::to_string(store_->increment(std::string(key), delta)));
}

Response CommandProcessor::handleSize(std::string_view arguments) const {
    if (!arguments.empty()) {
        return usage("SIZE");
    }
    return ok(std::to_string(store_->size()));
}

Response CommandProcessor::handleStats(std::string_view arguments) const {
    if (!arguments.empty()) {
        return usage("STATS");
    }
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - stats_->startedAt)
                            .count();

    std::ostringstream out;
    out << "connections_total=" << stats_->connectionsTotal.load()
        << " connections_active=" << stats_->connectionsActive.load()
        << " connections_rejected=" << stats_->connectionsRejected.load()
        << " commands_processed=" << stats_->commandsProcessed.load() << " keys=" << store_->size()
        << " uptime_seconds=" << uptime;
    return ok(out.str());
}

Response CommandProcessor::handleHelp(std::string_view /*arguments*/) const {
    return ok(
        "commands: PING, ECHO <text>, SET <key> <value>, GET <key>, DEL <key>, "
        "INCR <key> [delta], SIZE, STATS, HELP, QUIT");
}

Response CommandProcessor::handleQuit(std::string_view /*arguments*/) const { return {"BYE", true}; }

}  // namespace tcp
