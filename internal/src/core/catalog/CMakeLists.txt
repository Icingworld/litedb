set(CATALOG_SOURCES
    catalog_default_expression.cpp
    catalog_entry.cpp
    in_memory_catalog.cpp
)

add_library(litedb_catalog ${CATALOG_SOURCES})

target_include_directories(litedb_catalog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
