add_executable(parser_lexer_tests
    lexer_tests.cpp
)

target_link_libraries(parser_lexer_tests PRIVATE litedb_parser)

add_test(NAME parser_lexer_tests COMMAND parser_lexer_tests)

add_executable(parser_ast_tests
    ast_tests.cpp
)

target_link_libraries(parser_ast_tests PRIVATE litedb_parser)

add_test(NAME parser_ast_tests COMMAND parser_ast_tests)
