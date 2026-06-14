set(PARSER_SOURCES
    token.cpp
    lexer.cpp
    parser.cpp
    ast/ast_node.cpp
    ast/expression/expression_node.cpp
    ast/expression/identifier_expression.cpp
    ast/expression/literal_expression.cpp
    ast/expression/column_reference_expression.cpp
    ast/expression/wildcard_expression.cpp
    ast/expression/function_call_expression.cpp
    ast/expression/vector_expression.cpp
    ast/expression/binary_expression.cpp
    ast/expression/unary_expression.cpp
    ast/expression/in_expression.cpp
    ast/expression/between_expression.cpp
    ast/expression/like_expression.cpp
    ast/statement/statement_node.cpp
    ast/statement/select_statement.cpp
    ast/statement/insert_statement.cpp
    ast/statement/create_database_statement.cpp
    ast/statement/create_collection_statement.cpp
    ast/statement/create_index_statement.cpp
    ast/statement/create_vector_index_statement.cpp
    ast/statement/drop_database_statement.cpp
    ast/statement/drop_collection_statement.cpp
    ast/statement/drop_index_statement.cpp
    ast/statement/drop_vector_index_statement.cpp
    ast/statement/use_statement.cpp
    ast/statement/delete_statement.cpp
    ast/statement/update_statement.cpp
    ast/statement/show_statement.cpp
    ast/statement/describe_statement.cpp
    ast/statement/alter_statement.cpp
)

add_library(litedb_parser ${PARSER_SOURCES})

target_include_directories(litedb_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
