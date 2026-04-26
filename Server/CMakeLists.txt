cmake_minimum_required(VERSION 3.28.3)
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




add_library(SERVER_RUNTIME STATIC
        Runtime/Executor.cpp
        Runtime/ExpirationManager.cpp
        Runtime/StorageEngine.cpp
)
target_include_directories(SERVER_RUNTIME PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(SERVER_COMMANDS STATIC
        Commands/Statement.cpp
)
target_include_directories(SERVER_COMMANDS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_COMMANDS PUBLIC SERVER_RUNTIME)

add_library(SERVER_PARSING STATIC
        Parsing/Parser.cpp
        Parsing/Token.cpp
        Parsing/Tokenizer.cpp
        Utility/util.cpp
)
target_include_directories(SERVER_PARSING PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_PARSING PUBLIC SERVER_COMMANDS SERVER_RUNTIME)

add_library(SERVER_PROTOCOL STATIC
        Protocol/ResponseFormatter.cpp
)
target_include_directories(SERVER_PROTOCOL PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_PROTOCOL PUBLIC SERVER_COMMANDS SERVER_RUNTIME)

add_library(SERVER_APP STATIC
        App/CommandProcessor.cpp
)
target_include_directories(SERVER_APP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_APP PUBLIC SERVER_PARSING SERVER_RUNTIME SERVER_COMMANDS)

add_library(SERVER_PERSISTENCE STATIC
        Persistence/AOFLogger.cpp
        Persistence/LogRunner.cpp
)
target_include_directories(SERVER_PERSISTENCE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_PERSISTENCE PUBLIC SERVER_APP)

add_library(SERVER_TCP STATIC
        TCP/Server.cpp
)
target_include_directories(SERVER_TCP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_TCP PUBLIC SERVER_APP SERVER_PERSISTENCE SERVER_PROTOCOL)

add_executable(redisserver main.cpp)
target_link_libraries(redisserver
        PRIVATE
        SERVER_RUNTIME
        SERVER_COMMANDS
        SERVER_PARSING
        SERVER_PROTOCOL
        SERVER_APP
        SERVER_PERSISTENCE
        SERVER_TCP
)

add_executable(testTokenizer Tests/testTokenizer.cpp)
target_link_libraries(testTokenizer PRIVATE SERVER_PARSING SERVER_COMMANDS SERVER_RUNTIME GTest::gtest_main)

add_executable(testParser Tests/testParser.cpp)
target_link_libraries(testParser PRIVATE SERVER_PARSING SERVER_COMMANDS SERVER_RUNTIME GTest::gtest_main)

add_executable(testExecutor Tests/testExecutor.cpp)
target_link_libraries(testExecutor PRIVATE SERVER_RUNTIME SERVER_COMMANDS SERVER_PARSING GTest::gtest_main)

add_executable(testStorageEngine Tests/testStorageEngine.cpp)
target_link_libraries(testStorageEngine PRIVATE SERVER_RUNTIME GTest::gtest_main)

add_executable(testExpirationManager Tests/testExpirationManager.cpp)
target_link_libraries(testExpirationManager PRIVATE SERVER_RUNTIME GTest::gtest_main)

add_executable(testLogging Tests/testLogging.cpp)
target_link_libraries(testLogging
        PRIVATE
        SERVER_RUNTIME
        SERVER_COMMANDS
        SERVER_PARSING
        SERVER_APP
        SERVER_PERSISTENCE
        GTest::gtest_main
)

add_executable(testServerIntegration Tests/testServerIntegration.cpp)
target_link_libraries(testServerIntegration
        PRIVATE
        SERVER_RUNTIME
        SERVER_COMMANDS
        SERVER_PARSING
        SERVER_PROTOCOL
        SERVER_APP
        SERVER_PERSISTENCE
        SERVER_TCP
        GTest::gtest_main
)


include(GoogleTest)
gtest_discover_tests(testTokenizer)
gtest_discover_tests(testParser)
gtest_discover_tests(testExecutor)
gtest_discover_tests(testStorageEngine)
gtest_discover_tests(testExpirationManager)
gtest_discover_tests(testLogging)
gtest_discover_tests(testServerIntegration)
