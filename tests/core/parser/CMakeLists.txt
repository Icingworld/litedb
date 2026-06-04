add_executable(parser_lexer_tests
    lexer_tests.cpp
)

target_link_libraries(parser_lexer_tests PRIVATE litedb_parser)

add_test(NAME parser_lexer_tests COMMAND parser_lexer_tests)
