set(PHYSICAL_PLAN_SOURCES
    physical_planner.cpp
    node/physical_filter.cpp
    node/physical_index_scan.cpp
    node/physical_limit.cpp
    node/physical_plan_node.cpp
    node/physical_projection.cpp
    node/physical_seq_scan.cpp
    node/physical_sort.cpp
    node/physical_unary_node.cpp
    statement/physical_query_plan.cpp
    statement/physical_row_mutation_plan.cpp
    statement/physical_statement_plan.cpp
)

add_library(litedb_physical_plan ${PHYSICAL_PLAN_SOURCES})

target_include_directories(litedb_physical_plan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_physical_plan PUBLIC
    litedb_logical_plan
    litedb_binder
    litedb_index
)
