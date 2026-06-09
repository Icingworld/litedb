set(LOGICAL_PLANNER_SOURCES
    ../planner.cpp
    ../statement/create_collection_plan.cpp
    ../statement/create_database_plan.cpp
    ../statement/delete_plan.cpp
    ../statement/describe_collection_plan.cpp
    ../statement/drop_collection_plan.cpp
    ../statement/drop_database_plan.cpp
    ../statement/insert_plan.cpp
    ../statement/query_plan.cpp
    ../statement/show_collections_plan.cpp
    ../statement/show_databases_plan.cpp
    ../statement/statement_plan.cpp
    ../statement/update_plan.cpp
    ../statement/use_plan.cpp
    logical_filter.cpp
    logical_limit.cpp
    logical_order_by.cpp
    logical_plan_node.cpp
    logical_planner.cpp
    logical_projection.cpp
    logical_scan.cpp
    logical_unary_node.cpp
)

add_library(litedb_logical_planner ${LOGICAL_PLANNER_SOURCES})

target_include_directories(litedb_logical_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

target_link_libraries(litedb_logical_planner PUBLIC litedb_binder)
