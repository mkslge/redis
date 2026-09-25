add_library(SERVER_NETWORK STATIC
        ClientSession.cpp
        Server.cpp
)
target_link_libraries(SERVER_NETWORK PUBLIC SERVER_APP COMMON_SOCKET_IO)
