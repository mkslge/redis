add_executable(testClient testClient.cpp)
target_link_libraries(testClient PRIVATE CORE_CLIENT GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(testClient)