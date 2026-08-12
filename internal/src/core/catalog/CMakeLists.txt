set(CATALOG_SOURCES
    entry/collection_entry.cpp
    entry/column_entry.cpp
    entry/database_entry.cpp
    entry/index_entry.cpp
    entry/catalog_entry.cpp
    entry/vector_index_entry.cpp
    catalog_error.cpp
    catalog_state.cpp
    catalog_viewer.cpp
    catalog_editor.cpp
    catalog_publisher.cpp
    catalog_store.cpp
)

add_library(litedb_catalog ${CATALOG_SOURCES})

target_include_directories(litedb_catalog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_catalog PUBLIC
    litedb_io
    litedb_schema
)
