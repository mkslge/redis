add_library(SERVER_PROTOCOL STATIC
        ArgumentSplitter.cpp
        ByteEscaping.cpp
        ResponseFormatter.cpp
        RespCommandCodec.cpp
)
target_link_libraries(SERVER_PROTOCOL PUBLIC SERVER_COMMANDS)
