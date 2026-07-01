set(LOGICAL_PLANNER_SOURCES
    ../planner.cpp
    ../planner_helper.cpp
    ../plan/command/create_collection_plan.cpp
    ../plan/command/create_database_plan.cpp
    ../plan/command/create_index_plan.cpp
    ../plan/command/create_vector_index_plan.cpp
    ../plan/mutation/delete_plan.cpp
    ../plan/command/describe_collection_plan.cpp
    ../plan/command/drop_collection_plan.cpp
    ../plan/command/drop_database_plan.cpp
    ../plan/command/drop_index_plan.cpp
    ../plan/command/drop_vector_index_plan.cpp
    ../plan/mutation/insert_plan.cpp
    ../plan/query/query_plan.cpp
    ../plan/command/show_collections_plan.cpp
    ../plan/command/show_databases_plan.cpp
    ../plan/command/show_indexes_plan.cpp
    ../plan/command/show_vector_indexes_plan.cpp
    ../plan/statement_plan.cpp
    ../plan/mutation/update_plan.cpp
    ../plan/command/use_plan.cpp
    ../access_path/access_path_selector.cpp
    debug_printer.cpp
    logical_planner.cpp
    node/logical_filter.cpp
    node/logical_limit.cpp
    node/logical_order_by.cpp
    node/logical_plan_node.cpp
    node/logical_projection.cpp
    node/logical_scan.cpp
    node/logical_unary_node.cpp
)

add_library(litedb_logical_planner ${LOGICAL_PLANNER_SOURCES})

target_include_directories(litedb_logical_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

target_link_libraries(litedb_logical_planner PUBLIC
    litedb_binder
    litedb_index
)
