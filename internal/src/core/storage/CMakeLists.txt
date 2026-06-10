set(STORAGE_SOURCES
    in_memory_collection_storage.cpp
    storage_manager.cpp
)

add_library(litedb_storage ${STORAGE_SOURCES})

target_include_directories(litedb_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_storage PUBLIC litedb_schema)
