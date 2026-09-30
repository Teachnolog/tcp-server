#include "StringUtils.hpp"
#include "TcpClient.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

struct BenchOptions {
    std::string host = "127.0.0.1";
    std::uint16_t port = 8080;
    std::size_t clients = 8;
    std::size_t requests = 10'000;
    std::string key = "bench:counter";
    bool showHelp = false;
};

void printUsage(std::string_view program) {
    std::cout << "Usage: " << program
              << " [options]\n"
                 "  --host <address>   server address (default: 127.0.0.1)\n"
                 "  --port <port>      server port (default: 8080)\n"
                 "  --clients <n>      parallel connections (default: 8)\n"
                 "  --requests <n>     requests per connection (default: 10000)\n"
                 "  --key <key>        counter key (default: bench:counter)\n"
                 "\nNote: keep --clients <= server --threads, otherwise extra clients wait in the queue.\n";
}

template <typename Number>
Number parsePositive(std::string_view name, std::string_view value) {
    const auto parsed = tcp::parseNumber<Number>(value);
    if (!parsed || *parsed == 0) {
        throw std::invalid_argument("invalid value '" + std::string(value) + "' for " + std::string(name));
    }
    return *parsed;
}

BenchOptions parseOptions(int argc, char* argv[]) {
    BenchOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view name = argv[i];
        if (name == "-h" || name == "--help") {
            options.showHelp = true;
            continue;
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value for " + std::string(name));
        }
        const std::string_view value = argv[++i];
        if (name == "--host") {
            options.host = std::string(value);
        } else if (name == "--port") {
            options.port = parsePositive<std::uint16_t>(name, value);
        } else if (name == "--clients") {
            options.clients = parsePositive<std::size_t>(name, value);
        } else if (name == "--requests") {
            options.requests = parsePositive<std::size_t>(name, value);
        } else if (name == "--key") {
            options.key = std::string(value);
        } else {
            throw std::invalid_argument("unknown option '" + std::string(name) + "'");
        }
    }
    return options;
}

double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0.0;
    }
    const auto index = static_cast<std::size_t>(fraction * static_cast<double>(sorted.size() - 1));
    return sorted[index];
}

}  // namespace

int main(int argc, char* argv[]) {
    using Clock = std::chrono::steady_clock;

    BenchOptions options;
    try {
        options = parseOptions(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    if (options.showHelp) {
        printUsage(argv[0]);
        return EXIT_SUCCESS;
    }

    try {
        tcp::TcpClient control(options.host, options.port);
        control.request("DEL " + options.key);
    } catch (const std::exception& e) {
        std::cerr << "cannot reach server at " << options.host << ':' << options.port << ": " << e.what()
                  << '\n';
        return EXIT_FAILURE;
    }

    const std::string command = "INCR " + options.key;
    std::vector<std::vector<double>> latencies(options.clients);
    std::atomic<std::size_t> errors{0};
    std::mutex errorOutputMutex;

    std::promise<void> startSignal;
    const std::shared_future<void> start = startSignal.get_future().share();

    std::vector<std::thread> clients;
    clients.reserve(options.clients);
    for (std::size_t index = 0; index < options.clients; ++index) {
        clients.emplace_back([&, index] {
            std::vector<double>& samples = latencies[index];
            samples.reserve(options.requests);
            try {
                tcp::TcpClient client(options.host, options.port, std::chrono::seconds(30));
                start.wait();
                for (std::size_t r = 0; r < options.requests; ++r) {
                    const auto begin = Clock::now();
                    const std::string response = client.request(command);
                    samples.push_back(std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
                    if (response.rfind("OK ", 0) != 0) {
                        errors.fetch_add(1);
                    }
                }
            } catch (const std::exception& e) {
                errors.fetch_add(1);
                const std::lock_guard lock(errorOutputMutex);
                std::cerr << "client " << index << ": " << e.what() << '\n';
            }
        });
    }

    const auto begin = Clock::now();
    startSignal.set_value();
    for (std::thread& client : clients) {
        client.join();
    }
    const double elapsedSeconds = std::chrono::duration<double>(Clock::now() - begin).count();

    std::vector<double> all;
    for (const auto& samples : latencies) {
        all.insert(all.end(), samples.begin(), samples.end());
    }
    std::sort(all.begin(), all.end());

    const std::size_t expected = options.clients * options.requests;
    std::string counter = "?";
    try {
        tcp::TcpClient control(options.host, options.port);
        const std::string response = control.request("GET " + options.key);
        counter = response.rfind("OK ", 0) == 0 ? response.substr(3) : response;
    } catch (const std::exception& e) {
        std::cerr << "cannot read counter: " << e.what() << '\n';
    }
    const bool counterMatches = counter == std::to_string(expected);

    std::cout << std::fixed << std::setprecision(3)
              << "Clients:              " << options.clients << '\n'
              << "Requests per client:  " << options.requests << '\n'
              << "Completed requests:   " << all.size() << '\n'
              << "Errors:               " << errors.load() << '\n'
              << "Elapsed:              " << elapsedSeconds << " s\n"
              << "Throughput:           " << std::setprecision(0)
              << static_cast<double>(all.size()) / elapsedSeconds << " req/s\n"
              << std::setprecision(3)
              << "Latency p50 / p99:    " << percentile(all, 0.50) << " / " << percentile(all, 0.99) << " ms\n"
              << "Latency max:          " << (all.empty() ? 0.0 : all.back()) << " ms\n"
              << "Counter check:        " << counter << (counterMatches ? " == " : " != ") << expected
              << (counterMatches ? "  OK (no lost updates)" : "  MISMATCH") << '\n';

    return errors.load() == 0 && counterMatches ? EXIT_SUCCESS : EXIT_FAILURE;
}
