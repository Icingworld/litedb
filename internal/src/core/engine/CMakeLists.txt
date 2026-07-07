set(ENGINE_SOURCES
    database_instance.cpp
    engine.cpp
    session.cpp
)

add_library(litedb_engine ${ENGINE_SOURCES})

target_include_directories(litedb_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_engine PUBLIC
    litedb_parser
    litedb_binder
    litedb_logical_planner
    litedb_executor
    litedb_index
    litedb_persistence
)
