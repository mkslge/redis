add_library(SERVER_RUNTIME STATIC
        Executor.cpp
        ExpirationManager.cpp
        StorageEngine.cpp
)

target_include_directories(SERVER_RUNTIME PUBLIC ${PROJECT_SOURCE_DIR})
