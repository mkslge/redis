include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(testArgumentSplitter testArgumentSplitter.cpp)
target_link_libraries(testArgumentSplitter PRIVATE SERVER_PROTOCOL GTest::gtest_main)

add_executable(testParser testParser.cpp)
target_link_libraries(testParser PRIVATE SERVER_COMMANDS GTest::gtest_main)

add_executable(testExecutor testExecutor.cpp)
target_link_libraries(testExecutor PRIVATE SERVER_PROTOCOL GTest::gtest_main)

add_executable(testStorageEngine testStorageEngine.cpp)
target_link_libraries(testStorageEngine PRIVATE SERVER_STORAGE GTest::gtest_main)

add_executable(testCommandProcessor testCommandProcessor.cpp)
target_link_libraries(testCommandProcessor PRIVATE SERVER_APP GTest::gtest_main)

add_executable(testAofPersistence testAofPersistence.cpp)
target_link_libraries(testAofPersistence PRIVATE SERVER_APP GTest::gtest_main)

add_executable(testAofCompactor testAofCompactor.cpp)
target_link_libraries(testAofCompactor PRIVATE SERVER_APP GTest::gtest_main)

add_executable(testServerIntegration testServerIntegration.cpp)
target_link_libraries(testServerIntegration PRIVATE SERVER_NETWORK GTest::gtest_main)

add_executable(testRespCommandCodec testRespCommandCodec.cpp)
target_link_libraries(testRespCommandCodec PRIVATE SERVER_PROTOCOL GTest::gtest_main)

add_executable(testResponseFormatter testResponseFormatter.cpp)
target_link_libraries(testResponseFormatter PRIVATE SERVER_PROTOCOL GTest::gtest_main)

add_executable(testSocketIO testSocketIO.cpp)
target_link_libraries(testSocketIO PRIVATE COMMON_SOCKET_IO GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(testArgumentSplitter)
gtest_discover_tests(testParser)
gtest_discover_tests(testExecutor)
gtest_discover_tests(testStorageEngine)
gtest_discover_tests(testCommandProcessor)
gtest_discover_tests(testAofPersistence)
gtest_discover_tests(testAofCompactor)
gtest_discover_tests(testServerIntegration PROPERTIES TIMEOUT 10)
gtest_discover_tests(testRespCommandCodec)
gtest_discover_tests(testResponseFormatter)
gtest_discover_tests(testSocketIO)
