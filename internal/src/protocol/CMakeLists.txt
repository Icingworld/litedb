set(PROTOCOL_SOURCES
    message.cpp
)

add_library(litedb_protocol ${PROTOCOL_SOURCES})

target_include_directories(litedb_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(litedb_protocol PUBLIC
    litedb_executor
)
