set(LOGICAL_PLAN_SOURCES
    debug_printer.cpp
    logical_planner.cpp
    logical_planner_helper.cpp
    node/logical_filter.cpp
    node/logical_index_scan.cpp
    node/logical_limit.cpp
    node/logical_order_by.cpp
    node/logical_plan_node.cpp
    node/logical_projection.cpp
    node/logical_scan.cpp
    node/logical_unary_node.cpp
    statement/command/create_collection_plan.cpp
    statement/command/create_database_plan.cpp
    statement/command/create_index_plan.cpp
    statement/command/create_vector_index_plan.cpp
    statement/command/describe_collection_plan.cpp
    statement/command/drop_collection_plan.cpp
    statement/command/drop_database_plan.cpp
    statement/command/drop_index_plan.cpp
    statement/command/drop_vector_index_plan.cpp
    statement/command/show_collections_plan.cpp
    statement/command/show_databases_plan.cpp
    statement/command/show_indexes_plan.cpp
    statement/command/show_vector_indexes_plan.cpp
    statement/command/use_plan.cpp
    statement/mutation/delete_plan.cpp
    statement/mutation/insert_plan.cpp
    statement/mutation/update_plan.cpp
    statement/query/query_plan.cpp
    statement/statement_plan.cpp
)

add_library(litedb_logical_plan ${LOGICAL_PLAN_SOURCES})

target_include_directories(litedb_logical_plan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_logical_plan PUBLIC
    litedb_binder
    litedb_index
)

add_library(litedb_logical_planner ALIAS litedb_logical_plan)
