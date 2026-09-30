#include "Logger.hpp"
#include "ServerConfig.hpp"
#include "TcpServer.hpp"

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

int main(int argc, char* argv[]) {
    using namespace tcp;

    CommandLineOptions options;
    try {
        options = parseCommandLine(argc, argv);
    } catch (const std::invalid_argument& e) {
        std::cerr << "error: " << e.what() << "\n\n" << usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (options.showHelp) {
        std::cout << usage(argv[0]);
        return EXIT_SUCCESS;
    }
    Logger::setLevel(options.config.logLevel);

    // блокируем сигналы до создания потоков и ждем их в main через sigwait
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);
    signal(SIGPIPE, SIG_IGN);

    try {
        TcpServer server(options.config);

        std::thread serverThread([&server] {
            server.run();
            // если run() завершился сам, будим main
            kill(getpid(), SIGTERM);
        });

        int signalNumber = 0;
        sigwait(&signals, &signalNumber);
        logInfo("Received signal ", signalNumber, ", shutting down...");
        server.stop();
        serverThread.join();
    } catch (const std::exception& e) {
        logError("Fatal error: ", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
