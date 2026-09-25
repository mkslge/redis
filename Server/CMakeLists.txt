cmake_minimum_required(VERSION 3.28.3)
project(redisserver)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

enable_testing()

# Modules, in dependency order: each may use only the modules listed before it.
add_subdirectory(Core)
add_subdirectory(Storage)
add_subdirectory(Commands)
add_subdirectory(Protocol)
add_subdirectory(Persistence)
add_subdirectory(App)
add_subdirectory(Network)
add_subdirectory(Tests)

add_executable(redisserver main.cpp)
target_link_libraries(redisserver PRIVATE SERVER_NETWORK)
