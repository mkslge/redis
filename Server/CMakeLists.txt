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
add_subdirectory(Tests)

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
