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
        Interpreter/Executor/Executor.cpp
        Interpreter/Executor/StorageEngine.cpp
        Interpreter/Parsing/Parser.cpp
        Interpreter/Statements/Statement.cpp
        Interpreter/Tokens/Token.cpp
        Interpreter/Parsing/Tokenizer.cpp
        Utility/util.cpp
)
target_include_directories(SERVER_CORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(testTokenizer Tests/testTokenizer.cpp)
target_link_libraries(testTokenizer PRIVATE SERVER_CORE GTest::gtest_main)

add_executable(testParser Tests/testParser.cpp)
target_link_libraries(testParser PRIVATE SERVER_CORE GTest::gtest_main)

add_executable(testExecutor Tests/testExecutor.cpp)
target_link_libraries(testExecutor PRIVATE SERVER_CORE GTest::gtest_main)

add_executable(testStorageEngine Tests/testStorageEngine.cpp)
target_link_libraries(testStorageEngine PRIVATE SERVER_CORE GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(testTokenizer)
gtest_discover_tests(testParser)
gtest_discover_tests(testExecutor)
gtest_discover_tests(testStorageEngine)
