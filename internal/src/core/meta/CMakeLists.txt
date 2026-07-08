set(META_SOURCES
    entry/collection_entry.cpp
    entry/column_entry.cpp
    entry/database_entry.cpp
    entry/default_expression.cpp
    entry/index_entry.cpp
    entry/meta_entry.cpp
    entry/vector_index_entry.cpp
    meta_helper.cpp
)

add_library(litedb_meta ${META_SOURCES})

target_include_directories(litedb_meta PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
