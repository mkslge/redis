include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Adds one test executable built from <name>.cpp in the calling directory, linked to
# the module library it tests, and registers its tests with ctest. Any extra
# arguments are passed to gtest_discover_tests, e.g. PROPERTIES TIMEOUT 10.
function(add_server_test name library)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${library} GTest::gtest_main)
    gtest_discover_tests(${name} ${ARGN})
endfunction()

# One folder per module under test, mirroring the server's source layout.
add_subdirectory(Storage)
add_subdirectory(Commands)
add_subdirectory(Protocol)
add_subdirectory(Persistence)
add_subdirectory(App)
add_subdirectory(Network)
add_subdirectory(Common)
