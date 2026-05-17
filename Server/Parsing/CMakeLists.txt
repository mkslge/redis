add_library(SERVER_PARSING STATIC
        Parser.cpp
        Token.cpp
        Tokenizer.cpp
        ../Utility/util.cpp
)
target_include_directories(SERVER_PARSING PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(SERVER_PARSING PUBLIC SERVER_COMMANDS SERVER_RUNTIME)
