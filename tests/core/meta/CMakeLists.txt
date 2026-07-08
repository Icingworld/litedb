add_executable(meta_entry_tests
    meta_entry_tests.cpp
)

target_link_libraries(meta_entry_tests PRIVATE litedb_meta)

add_test(NAME meta_entry_tests COMMAND meta_entry_tests)
