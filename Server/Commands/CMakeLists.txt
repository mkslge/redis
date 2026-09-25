add_library(SERVER_COMMANDS STATIC
        Command.cpp
        Executor.cpp
        Parser.cpp
)
target_link_libraries(SERVER_COMMANDS PUBLIC SERVER_STORAGE)
