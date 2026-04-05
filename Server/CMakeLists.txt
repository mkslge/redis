cmake_minimum_required(VERSION 3.31)
project(redisserver)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()




add_library(SERVER_CORE STATIC
        Interpreter/Parser.cpp
        Interpreter/Tokenizer.cpp
        Utility/util.cpp
)

add_executable(testTokenizer Tests/testTokenizer.cpp)
target_link_libraries(testTokenizer PRIVATE SERVER_CORE GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(testTokenizer)