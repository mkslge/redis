cmake_minimum_required(VERSION 3.31)
project(redisclient)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Server/build/_deps/googletest-src/CMakeLists.txt")
    FetchContent_Declare(
            googletest
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Server/build/_deps/googletest-src"
    )
else()
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
endif()
FetchContent_MakeAvailable(googletest)
enable_testing()

add_library(CORE_CLIENT Networking/Client.cpp)
target_include_directories(CORE_CLIENT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(redisclient main.cpp)
target_link_libraries(redisclient PRIVATE CORE_CLIENT)

add_executable(testClient Tests/testClient.cpp)
target_link_libraries(testClient PRIVATE CORE_CLIENT GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(testClient)
