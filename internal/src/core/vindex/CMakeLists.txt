add_subdirectory(flat_index)
add_subdirectory(hnsw_index)

set(VINDEX_SOURCES
    ${FLAT_INDEX_SOURCES}
    vector_distance.cpp
    vector_index_key.cpp
    vector_index_manager.cpp
)

add_library(litedb_vindex ${VINDEX_SOURCES})

target_include_directories(litedb_vindex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_vindex PUBLIC
    litedb_schema
    litedb_storage
)
