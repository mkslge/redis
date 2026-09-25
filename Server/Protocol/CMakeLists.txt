add_library(SERVER_PROTOCOL STATIC
        ArgumentSplitter.cpp
        ResponseFormatter.cpp
        RespCommandCodec.cpp
)
target_link_libraries(SERVER_PROTOCOL PUBLIC SERVER_COMMANDS)
