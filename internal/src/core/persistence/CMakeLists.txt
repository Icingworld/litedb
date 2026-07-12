set(PERSISTENCE_SOURCES
    manifest_store.cpp
    persistent_collection_storage.cpp
    persistence_controller.cpp
    row_log.cpp
)

add_library(litedb_persistence ${PERSISTENCE_SOURCES})

target_include_directories(litedb_persistence PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_persistence PUBLIC
    litedb_meta
    litedb_index
    litedb_io
    litedb_schema
    litedb_storage
    litedb_executor
)
