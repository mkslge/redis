# Vocabulary types shared by every module. Also provides the single include root,
# so headers are always included by module path, e.g. "Commands/Parser.h".
add_library(SERVER_CORE INTERFACE)
target_include_directories(SERVER_CORE INTERFACE ${PROJECT_SOURCE_DIR})
