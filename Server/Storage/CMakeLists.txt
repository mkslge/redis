add_library(SERVER_STORAGE STATIC
        StorageEngine.cpp
)
target_link_libraries(SERVER_STORAGE PUBLIC SERVER_CORE)
