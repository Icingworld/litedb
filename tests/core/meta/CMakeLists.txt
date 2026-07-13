add_executable(meta_entry_tests
    meta_entry_tests.cpp
)

target_link_libraries(meta_entry_tests PRIVATE litedb_meta)

add_test(NAME meta_entry_tests COMMAND meta_entry_tests)

add_executable(meta_store_tests
    meta_store_tests.cpp
)

target_link_libraries(meta_store_tests PRIVATE
    litedb_meta
    litedb_filesystem
)

add_test(NAME meta_store_tests COMMAND meta_store_tests)

add_executable(meta_engine_tests
    meta_engine_tests.cpp
)

target_link_libraries(meta_engine_tests PRIVATE
    litedb_meta
    litedb_filesystem
)

add_test(NAME meta_engine_tests COMMAND meta_engine_tests)
