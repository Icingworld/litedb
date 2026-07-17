set(DATABASE_SOURCES
    database_engine.cpp
    database_manifest.cpp
    session.cpp
)

add_library(litedb_database ${DATABASE_SOURCES})

target_include_directories(litedb_database PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_database PUBLIC
    litedb_parser
    litedb_binder
    litedb_logical_plan
    litedb_optimizer
    litedb_physical_plan
    litedb_executor
    litedb_filesystem
    litedb_io
    litedb_index
    litedb_meta
    litedb_schema
    litedb_storage
    litedb_vindex
)
