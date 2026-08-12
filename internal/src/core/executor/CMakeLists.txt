set(EXECUTOR_SOURCES
    executor.cpp
    executor_detail.cpp
    command_executor.cpp
    mutation_executor.cpp
    query_executor.cpp
    physical_operator_executor.cpp
)

add_library(litedb_executor ${EXECUTOR_SOURCES})

target_include_directories(litedb_executor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_executor PUBLIC
    litedb_physical_planner
    litedb_evaluator
    litedb_index
    litedb_storage
    litedb_catalog
    litedb_vindex
    litedb_transaction
)
