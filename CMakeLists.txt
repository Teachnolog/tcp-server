cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

project(TCPServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT UNIX)
    message(FATAL_ERROR "POSIX sockets are required: build on Linux/macOS, WSL or Docker")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TCP_SERVER_BUILD_TESTS "Build tests" ON)
option(TCP_SERVER_BUILD_BENCH "Build bench_app" ON)
set(TCP_SERVER_SANITIZER "" CACHE STRING "address, thread or undefined")

find_package(Threads REQUIRED)

if(TCP_SERVER_SANITIZER)
    add_compile_options(-fsanitize=${TCP_SERVER_SANITIZER} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${TCP_SERVER_SANITIZER})
endif()

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(tcp_core STATIC
    src/CommandProcessor.cpp
    src/KeyValueStore.cpp
    src/LineBuffer.cpp
    src/Logger.cpp
    src/ServerConfig.cpp
    src/Socket.cpp
    src/TcpClient.cpp
    src/TcpServer.cpp
    src/ThreadPool.cpp
)
target_include_directories(tcp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tcp_core PUBLIC Threads::Threads PRIVATE project_warnings)

add_executable(server_app src/main.cpp)
target_link_libraries(server_app PRIVATE tcp_core project_warnings)

if(TCP_SERVER_BUILD_BENCH)
    add_executable(bench_app tools/bench.cpp)
    target_link_libraries(bench_app PRIVATE tcp_core project_warnings)
endif()

if(TCP_SERVER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
