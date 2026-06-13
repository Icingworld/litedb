add_executable(scalar_index_tests
    scalar_index_tests.cpp
)

target_link_libraries(scalar_index_tests PRIVATE litedb_index)

add_test(NAME scalar_index_tests COMMAND scalar_index_tests)

add_executable(index_manager_tests
    index_manager_tests.cpp
)

target_link_libraries(index_manager_tests PRIVATE litedb_index)

add_test(NAME index_manager_tests COMMAND index_manager_tests)
