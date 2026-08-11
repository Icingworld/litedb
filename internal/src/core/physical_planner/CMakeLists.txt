set(PHYSICAL_PLANNER_SOURCES
    access/scalar_access_path_selector.cpp
    access/vector_top_k_selector.cpp
    operator/debug/debug_printer.cpp
    operator/physical_filter_operator.cpp
    operator/physical_index_scan_operator.cpp
    operator/physical_limit_operator.cpp
    operator/physical_operator.cpp
    operator/physical_projection_operator.cpp
    operator/physical_seq_scan_operator.cpp
    operator/physical_sort_operator.cpp
    operator/physical_unary_operator.cpp
    operator/physical_vector_search_operator.cpp
    plan/physical_plan.cpp
    plan/command/create_collection_plan.cpp
    plan/command/create_database_plan.cpp
    plan/command/create_index_plan.cpp
    plan/command/create_vector_index_plan.cpp
    plan/command/describe_collection_plan.cpp
    plan/command/drop_collection_plan.cpp
    plan/command/drop_database_plan.cpp
    plan/command/drop_index_plan.cpp
    plan/command/drop_vector_index_plan.cpp
    plan/command/show_collections_plan.cpp
    plan/command/show_databases_plan.cpp
    plan/command/show_indexes_plan.cpp
    plan/command/show_vector_indexes_plan.cpp
    plan/command/use_plan.cpp
    plan/debug/debug_printer.cpp
    plan/mutation/delete_plan.cpp
    plan/mutation/insert_plan.cpp
    plan/mutation/update_plan.cpp
    plan/query/query_plan.cpp
    worker/physical_command_worker.cpp
    worker/physical_mutation_worker.cpp
    worker/physical_operator_worker.cpp
    worker/physical_planner_worker.cpp
    worker/physical_query_worker.cpp
    physical_planner_context.cpp
    physical_planner.cpp
)

add_library(litedb_physical_planner ${PHYSICAL_PLANNER_SOURCES})

target_include_directories(litedb_physical_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_physical_planner PUBLIC
    litedb_logical_planner
    litedb_binder
    litedb_meta
    litedb_evaluator
    litedb_index
)
