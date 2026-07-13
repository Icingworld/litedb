set(STORAGE_SOURCES
    storage_cursor.cpp
    storage_engine.cpp
    storage_store.cpp
)

add_library(litedb_storage ${STORAGE_SOURCES})

target_include_directories(litedb_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_storage PUBLIC litedb_schema litedb_filesystem litedb_io)
