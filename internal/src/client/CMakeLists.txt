set(CLIENT_SOURCES
    client.cpp
)

add_library(litedb_client ${CLIENT_SOURCES})

target_include_directories(litedb_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(litedb_client PUBLIC
    litedb_net
)
