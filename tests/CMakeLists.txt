include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(unit_tests
    test_command_processor.cpp
    test_key_value_store.cpp
    test_line_buffer.cpp
    test_server_config.cpp
    test_thread_pool.cpp
)
target_link_libraries(unit_tests PRIVATE tcp_core GTest::gtest_main)
target_compile_options(unit_tests PRIVATE -Wall -Wextra)
gtest_discover_tests(unit_tests DISCOVERY_TIMEOUT 30)

add_executable(integration_tests test_tcp_server.cpp)
target_link_libraries(integration_tests PRIVATE tcp_core GTest::gtest_main)
target_compile_options(integration_tests PRIVATE -Wall -Wextra)
gtest_discover_tests(integration_tests DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 60)
