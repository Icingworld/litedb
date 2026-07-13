add_library(litedb_io
    byte_reader.cpp
    binary_io.cpp
    buffer_byte_reader.cpp
    buffer_byte_writer.cpp
    file_byte_reader.cpp
    file_byte_writer.cpp
    io_helper.cpp
)

target_include_directories(litedb_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_io PUBLIC
    litedb_filesystem
    litedb_schema
)
