set(BINDER_SOURCES
    binder.cpp
    binder_helper.cpp
    binder_context.cpp
    worker/binder_worker.cpp
    worker/binder_worker_helper.cpp
    worker/binder_use_worker.cpp
    worker/binder_create_worker.cpp
    worker/binder_drop_worker.cpp
    worker/binder_show_worker.cpp
    worker/binder_describe_worker.cpp
    worker/binder_select_worker.cpp
    worker/binder_insert_worker.cpp
    worker/binder_update_worker.cpp
    worker/binder_delete_worker.cpp
    bound/expression/bound_binary_expression.cpp
    bound/expression/bound_between_expression.cpp
    bound/expression/bound_cast_expression.cpp
    bound/expression/bound_column_ref_expression.cpp
    bound/expression/bound_expression.cpp
    bound/expression/bound_function_expression.cpp
    bound/expression/bound_in_expression.cpp
    bound/expression/bound_like_expression.cpp
    bound/expression/bound_literal_expression.cpp
    bound/expression/bound_null_expression.cpp
    bound/expression/bound_unary_expression.cpp
    bound/expression/bound_vector_expression.cpp
    bound/statement/bound_create_collection_statement.cpp
    bound/statement/bound_create_database_statement.cpp
    bound/statement/bound_create_index_statement.cpp
    bound/statement/bound_create_vector_index_statement.cpp
    bound/statement/bound_delete_statement.cpp
    bound/statement/bound_describe_collection_statement.cpp
    bound/statement/bound_drop_collection_statement.cpp
    bound/statement/bound_drop_database_statement.cpp
    bound/statement/bound_drop_index_statement.cpp
    bound/statement/bound_drop_vector_index_statement.cpp
    bound/statement/bound_insert_statement.cpp
    bound/statement/bound_select_statement.cpp
    bound/statement/bound_show_collections_statement.cpp
    bound/statement/bound_show_indexes_statement.cpp
    bound/statement/bound_show_vector_indexes_statement.cpp
    bound/statement/bound_statement.cpp
    bound/statement/bound_update_statement.cpp
    bound/statement/bound_use_statement.cpp
)

add_library(litedb_binder ${BINDER_SOURCES})

target_include_directories(litedb_binder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_binder PUBLIC litedb_parser litedb_meta litedb_function)
