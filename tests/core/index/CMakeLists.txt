add_executable(scalar_index_tests
    scalar_index_tests.cpp
)

target_link_libraries(scalar_index_tests PRIVATE litedb_index)

add_test(NAME scalar_index_tests COMMAND scalar_index_tests)
