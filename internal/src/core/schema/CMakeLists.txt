set(SCHEMA_SOURCES
    collection.cpp
    column.cpp
    database.cpp
    schema_loader.cpp
    value.cpp
)

add_library(litedb_schema ${SCHEMA_SOURCES})

target_include_directories(litedb_schema PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_schema PUBLIC litedb_catalog)
