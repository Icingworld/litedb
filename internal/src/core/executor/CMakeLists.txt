set(EXECUTOR_SOURCES
    executor.cpp
)

add_library(litedb_executor ${EXECUTOR_SOURCES})

target_include_directories(litedb_executor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_executor PUBLIC
    litedb_logical_planner
    litedb_evaluator
    litedb_storage
    litedb_catalog
)
