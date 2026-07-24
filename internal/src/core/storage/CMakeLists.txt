set(STORAGE_SOURCES
    schema_loader.cpp
    storage_cursor.cpp
    storage_engine.cpp
    storage_page_codec.cpp
    storage_store.cpp
    value_codec.cpp
)

add_library(litedb_storage ${STORAGE_SOURCES})

target_include_directories(litedb_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_storage PUBLIC litedb_schema litedb_meta litedb_filesystem litedb_io)
