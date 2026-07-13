set(OPTIMIZER_SOURCES
    optimizer.cpp
)

add_library(litedb_optimizer ${OPTIMIZER_SOURCES})

target_include_directories(litedb_optimizer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_optimizer PUBLIC
    litedb_meta
    litedb_logical_plan
    litedb_evaluator
)
