set(LOGICAL_PLANNER_SOURCES
    logical_plan_node.cpp
    logical_planner.cpp
)

add_library(litedb_logical_planner ${LOGICAL_PLANNER_SOURCES})

target_include_directories(litedb_logical_planner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

target_link_libraries(litedb_logical_planner PUBLIC litedb_binder)
