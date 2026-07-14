set(INDEX_SOURCES
    btree_index.cpp
    hash_index.cpp
    index_engine.cpp
    index_store.cpp
    scalar_index.cpp
    scalar_index_key.cpp
)

add_library(litedb_index ${INDEX_SOURCES})

target_include_directories(litedb_index PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_index PUBLIC
    litedb_schema
    litedb_meta
    litedb_storage
)
