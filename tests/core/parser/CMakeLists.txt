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

add_executable(parser_debug_printer_tests
    debug_printer_tests.cpp
)

target_link_libraries(parser_debug_printer_tests PRIVATE litedb_parser)

add_test(NAME parser_debug_printer_tests COMMAND parser_debug_printer_tests)

add_executable(parser_parser_tests
    parser_tests.cpp
)

target_link_libraries(parser_parser_tests PRIVATE litedb_parser)

add_test(NAME parser_parser_tests COMMAND parser_parser_tests)

add_executable(parser_depth_tests
    parser_depth_tests.cpp
)

target_link_libraries(parser_depth_tests PRIVATE litedb_parser)

add_test(NAME parser_depth_tests COMMAND parser_depth_tests)
