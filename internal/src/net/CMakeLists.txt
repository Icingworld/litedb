set(NET_SOURCES
    frame_io.cpp
)

add_library(litedb_net ${NET_SOURCES})

target_include_directories(litedb_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(litedb_net PUBLIC
    asio
    litedb_protocol
)
