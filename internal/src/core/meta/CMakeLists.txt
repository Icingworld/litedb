set(META_SOURCES
    entry/collection_entry.cpp
    entry/column_entry.cpp
    entry/database_entry.cpp
    entry/index_entry.cpp
    entry/meta_entry.cpp
    entry/vector_index_entry.cpp
    meta_error.cpp
    meta_engine.cpp
    meta_store.cpp
)

add_library(litedb_meta ${META_SOURCES})

target_include_directories(litedb_meta PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_meta PUBLIC
    litedb_io
    litedb_schema
)
