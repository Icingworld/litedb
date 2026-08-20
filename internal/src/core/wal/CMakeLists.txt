set(WAL_SOURCES
    file_write_batch.cpp
    recovery_manager.cpp
    wal_codec.cpp
    wal_manager.cpp
    wal_store.cpp
)

add_library(litedb_wal ${WAL_SOURCES})

target_include_directories(litedb_wal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_wal PUBLIC litedb_filesystem PRIVATE litedb_io)
