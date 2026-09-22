add_library(CORE_CLIENT STATIC 
    Client.cpp)
target_include_directories(CORE_CLIENT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CORE_CLIENT PUBLIC REDISIMPL_SOCKET_IO)
