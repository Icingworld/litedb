add_executable(catalog_entry_tests
    catalog_entry_tests.cpp
)

target_link_libraries(catalog_entry_tests PRIVATE litedb_catalog)

add_test(NAME catalog_entry_tests COMMAND catalog_entry_tests)

add_executable(catalog_store_tests
    catalog_store_tests.cpp
)

target_link_libraries(catalog_store_tests PRIVATE
    litedb_catalog
    litedb_filesystem
)

add_test(NAME catalog_store_tests COMMAND catalog_store_tests)

add_executable(catalog_engine_tests
    catalog_engine_tests.cpp
)

target_link_libraries(catalog_engine_tests PRIVATE
    litedb_catalog
    litedb_filesystem
)

add_test(NAME catalog_engine_tests COMMAND catalog_engine_tests)
