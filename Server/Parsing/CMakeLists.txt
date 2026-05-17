add_library(SERVER_PARSING STATIC
        Parser.cpp
        Token.cpp
        Tokenizer.cpp
)
target_include_directories(SERVER_PARSING PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SERVER_PARSING PUBLIC SERVER_COMMANDS SERVER_RUNTIME SERVER_UTILS)
