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


add_subdirectory(Utility)
add_subdirectory(Runtime)
add_subdirectory(Commands)
add_subdirectory(Parsing)
add_subdirectory(Protocol)
add_subdirectory(App)
add_subdirectory(Persistence)
add_subdirectory(TCP)

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

add_executable(testLogCompactor Tests/testLogCompactor.cpp)
target_link_libraries(testLogCompactor
        PRIVATE
        SERVER_RUNTIME
        SERVER_COMMANDS
        SERVER_PARSING
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
gtest_discover_tests(testLogCompactor)
gtest_discover_tests(testServerIntegration PROPERTIES TIMEOUT 10)
