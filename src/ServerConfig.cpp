#include "ServerConfig.hpp"

#include "StringUtils.hpp"

#include <optional>
#include <stdexcept>

namespace tcp {
namespace {

template <typename Number>
Number parseOption(std::string_view name, std::string_view value, Number min, Number max) {
    const auto parsed = parseNumber<Number>(value);
    if (!parsed || *parsed < min || *parsed > max) {
        throw std::invalid_argument("invalid value '" + std::string(value) + "' for " + std::string(name) +
                                    " (expected " + std::to_string(min) + ".." + std::to_string(max) + ")");
    }
    return *parsed;
}

}  // namespace

CommandLineOptions parseCommandLine(int argc, const char* const* argv) {
    CommandLineOptions options;
    ServerConfig& config = options.config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            options.showHelp = true;
            continue;
        }

        std::string_view name = argument;
        std::optional<std::string_view> inlineValue;
        if (const auto equals = argument.find('='); equals != std::string_view::npos) {
            name = argument.substr(0, equals);
            inlineValue = argument.substr(equals + 1);
        }

        const auto value = [&]() -> std::string_view {
            if (inlineValue) {
                return *inlineValue;
            }
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for " + std::string(name));
            }
            return argv[++i];
        };

        if (name == "--host") {
            config.host = std::string(value());
        } else if (name == "--port") {
            config.port = parseOption<std::uint16_t>(name, value(), 0, 65535);
        } else if (name == "--threads") {
            config.threadCount = parseOption<std::size_t>(name, value(), 1, 1024);
        } else if (name == "--queue") {
            config.maxQueuedConnections = parseOption<std::size_t>(name, value(), 0, 1'000'000);
        } else if (name == "--idle-timeout") {
            config.idleTimeout = std::chrono::seconds(parseOption<long>(name, value(), 0, 86'400));
        } else if (name == "--max-line") {
            config.maxLineLength = parseOption<std::size_t>(name, value(), 16, 1 << 20);
        } else if (name == "--log-level") {
            const std::string_view levelName = value();
            const auto level = parseLogLevel(levelName);
            if (!level) {
                throw std::invalid_argument("invalid log level '" + std::string(levelName) + "'");
            }
            config.logLevel = *level;
        } else {
            throw std::invalid_argument("unknown option '" + std::string(argument) + "'");
        }
    }
    return options;
}

std::string usage(std::string_view programName) {
    return "Usage: " + std::string(programName) +
           " [options]\n"
           "\n"
           "Options:\n"
           "  --host <address>      IPv4 address to bind (default: 0.0.0.0)\n"
           "  --port <port>         TCP port, 0 = any free port (default: 8080)\n"
           "  --threads <n>         worker threads in the pool (default: CPU cores)\n"
           "  --queue <n>           max clients waiting for a free worker, 0 = unlimited (default: 1024)\n"
           "  --idle-timeout <sec>  close idle connections after N seconds, 0 = never (default: 60)\n"
           "  --max-line <bytes>    max length of a request line (default: 4096)\n"
           "  --log-level <level>   debug | info | warning | error | off (default: info)\n"
           "  -h, --help            show this help\n";
}

}  // namespace tcp
