add_library(SERVER_TCP STATIC
        Server.cpp
)
target_include_directories(SERVER_TCP PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(SERVER_TCP PUBLIC SERVER_APP SERVER_PERSISTENCE SERVER_PROTOCOL)