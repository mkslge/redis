add_library(SERVER_PROTOCOL STATIC
        ResponseFormatter.cpp
)
target_include_directories(SERVER_PROTOCOL PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(SERVER_PROTOCOL PUBLIC SERVER_COMMANDS SERVER_RUNTIME)