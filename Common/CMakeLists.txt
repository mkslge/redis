add_library(REDISIMPL_SOCKET_IO STATIC
        Networking/SocketIO.cpp
)

target_include_directories(REDISIMPL_SOCKET_IO PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Networking
)
