add_library(SERVER_APP STATIC
        CommandProcessor.cpp
)
target_link_libraries(SERVER_APP PUBLIC SERVER_PROTOCOL)
