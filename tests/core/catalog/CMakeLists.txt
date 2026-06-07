add_executable(catalog_in_memory_tests
    in_memory_catalog_tests.cpp
)

target_link_libraries(catalog_in_memory_tests PRIVATE litedb_catalog)

add_test(NAME catalog_in_memory_tests COMMAND catalog_in_memory_tests)
