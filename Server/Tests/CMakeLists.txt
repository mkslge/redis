
add_executable(testTokenizer testTokenizer.cpp)
target_link_libraries(testTokenizer PRIVATE SERVER_PARSING SERVER_COMMANDS SERVER_RUNTIME GTest::gtest_main)

add_executable(testParser testParser.cpp)
target_link_libraries(testParser PRIVATE SERVER_PARSING SERVER_COMMANDS SERVER_RUNTIME GTest::gtest_main)

add_executable(testExecutor testExecutor.cpp)
target_link_libraries(testExecutor PRIVATE SERVER_RUNTIME SERVER_COMMANDS SERVER_PARSING GTest::gtest_main)

add_executable(testStorageEngine testStorageEngine.cpp)
target_link_libraries(testStorageEngine PRIVATE SERVER_RUNTIME GTest::gtest_main)

add_executable(testExpirationManager testExpirationManager.cpp)
target_link_libraries(testExpirationManager PRIVATE SERVER_RUNTIME GTest::gtest_main)

add_executable(testLogging testLogging.cpp)
target_link_libraries(testLogging
        PRIVATE
        SERVER_RUNTIME
        SERVER_COMMANDS
        SERVER_PARSING
        SERVER_APP
        SERVER_PERSISTENCE
        GTest::gtest_main
)

add_executable(testLogCompactor testLogCompactor.cpp)
target_link_libraries(testLogCompactor
        PRIVATE
        SERVER_RUNTIME
        SERVER_COMMANDS
        SERVER_PARSING
        SERVER_PERSISTENCE
        GTest::gtest_main
)

add_executable(testServerIntegration testServerIntegration.cpp)
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
