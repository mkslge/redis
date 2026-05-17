add_library(SERVER_UTILS STATIC
    util.cpp
)


target_include_directories(SERVER_UTILS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
