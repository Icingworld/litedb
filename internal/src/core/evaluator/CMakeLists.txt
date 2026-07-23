set(EVALUATOR_SOURCES
    expression_evaluator.cpp
)

add_library(litedb_evaluator ${EVALUATOR_SOURCES})

target_include_directories(litedb_evaluator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_evaluator PUBLIC litedb_binder litedb_common litedb_function)
