set(PHYSICAL_PLANNER_SOURCES
    physical_planner.cpp
    node/physical_filter.cpp
    node/physical_index_scan.cpp
    node/physical_limit.cpp
    node/physical_plan_node.cpp
    node/physical_projection.cpp
    node/physical_seq_scan.cpp
    node/physical_sort.cpp
    node/physical_unary_node.cpp
    node/physical_vector_search.cpp
    statement/physical_query_plan.cpp
    statement/physical_row_mutation_plan.cpp
    statement/physical_statement_plan.cpp
)

add_library(litedb_physical_planner ${PHYSICAL_PLANNER_SOURCES})

target_include_directories(litedb_physical_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_physical_planner PUBLIC
    litedb_logical_planner
    litedb_binder
    litedb_index
)
