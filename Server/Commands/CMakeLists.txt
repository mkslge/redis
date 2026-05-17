add_library(SERVER_COMMANDS STATIC
        Statement.cpp
)

target_include_directories(SERVER_COMMANDS
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(SERVER_COMMANDS
    PUBLIC
    SERVER_RUNTIME
)