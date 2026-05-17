add_library(SERVER_APP STATIC
        CommandProcessor.cpp
)


target_include_directories(SERVER_APP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_APP PUBLIC SERVER_PARSING SERVER_RUNTIME SERVER_COMMANDS)