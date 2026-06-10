add_executable(binder_tests
    binder_tests.cpp
)

target_link_libraries(binder_tests PRIVATE litedb_binder)

add_test(NAME binder_tests COMMAND binder_tests)
