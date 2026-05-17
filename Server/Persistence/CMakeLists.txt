add_library(SERVER_PERSISTENCE STATIC
        AOFLogger.cpp
        LogRunner.cpp
        LogCompactor.cpp
)
target_include_directories(SERVER_PERSISTENCE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_PERSISTENCE PUBLIC SERVER_APP)
