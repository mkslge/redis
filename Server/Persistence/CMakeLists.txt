add_library(SERVER_PERSISTENCE STATIC
        AOFLogger.cpp
        AofRecords.cpp
        LogCompactor.cpp
        LogRunner.cpp
)
target_link_libraries(SERVER_PERSISTENCE PUBLIC SERVER_PROTOCOL)
