cmake_minimum_required(VERSION 3.28.3)
project(redisclient)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build optimized, with debug symbols, unless a build type was chosen explicitly
# (-DCMAKE_BUILD_TYPE=...). Multi-config generators such as Xcode choose per build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Common ${CMAKE_CURRENT_BINARY_DIR}/Common)

include(FetchContent)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Server/build/_deps/googletest-src/CMakeLists.txt")
    FetchContent_Declare(
            googletest
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Server/build/_deps/googletest-src"
    )
else()
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
endif()

FetchContent_MakeAvailable(googletest)
enable_testing()
add_subdirectory(Networking)
add_subdirectory(Tests)

add_executable(redisclient main.cpp)
target_link_libraries(redisclient PRIVATE CLIENT_NETWORKING)
