add_library(SERVER_PERSISTENCE STATIC
        AOFLogger.cpp
        LogRunner.cpp
        LogCompactor.cpp
)
target_link_libraries(SERVER_PERSISTENCE PUBLIC SERVER_APP)
