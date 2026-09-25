add_library(COMMON_SOCKET_IO STATIC
        Networking/SocketIO.cpp
)
target_include_directories(COMMON_SOCKET_IO PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
