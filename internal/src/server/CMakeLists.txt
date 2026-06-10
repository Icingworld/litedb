set(SERVER_SOURCES
    server.cpp
)

add_library(litedb_server ${SERVER_SOURCES})

target_include_directories(litedb_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(litedb_server PUBLIC
    litedb_engine
    litedb_net
)
