add_library(SERVER_PERSISTENCE STATIC
        AofWriter.cpp
        AofRecords.cpp
        AofCompactor.cpp
        AofReplayer.cpp
)
target_link_libraries(SERVER_PERSISTENCE PUBLIC SERVER_PROTOCOL)
