add_library(litedb_filesystem
    file_handle.cpp
    filesystem.cpp
)

target_include_directories(litedb_filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
