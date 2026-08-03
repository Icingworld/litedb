set(LOGICAL_PLANNER_SOURCES
    logical_planner.cpp
    logical_planner_helper.cpp
    worker/logical_planner_create_worker.cpp
    worker/logical_planner_delete_worker.cpp
    worker/logical_planner_describe_worker.cpp
    worker/logical_planner_drop_worker.cpp
    worker/logical_planner_insert_worker.cpp
    worker/logical_planner_select_worker.cpp
    worker/logical_planner_show_worker.cpp
    worker/logical_planner_update_worker.cpp
    worker/logical_planner_use_worker.cpp
    worker/logical_planner_worker.cpp
    operator/debug/debug_printer.cpp
    operator/logical_filter_operator.cpp
    operator/logical_limit_operator.cpp
    operator/logical_order_by_operator.cpp
    operator/logical_plan_operator.cpp
    operator/logical_projection_operator.cpp
    operator/logical_scan_operator.cpp
    operator/logical_unary_operator.cpp
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
    plan/logical_plan.cpp
    plan/mutation/delete_plan.cpp
    plan/mutation/insert_plan.cpp
    plan/mutation/update_plan.cpp
    plan/query/query_plan.cpp
)

add_library(litedb_logical_planner ${LOGICAL_PLANNER_SOURCES})

target_include_directories(litedb_logical_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_logical_planner PUBLIC
    litedb_binder
    litedb_index
)
