set(PHYSICAL_PLANNER_SOURCES
    operator/debug/debug_printer.cpp
    plan/debug/debug_printer.cpp
    physical_planner_new.cpp
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
