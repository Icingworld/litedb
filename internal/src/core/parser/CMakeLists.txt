set(PARSER_SOURCES
    token.cpp
    lexer.cpp
)

add_library(litedb_parser ${PARSER_SOURCES})

target_include_directories(litedb_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
