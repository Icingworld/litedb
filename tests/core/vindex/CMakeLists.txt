add_executable(vector_index_tests
    vector_index_tests.cpp
)

target_link_libraries(vector_index_tests PRIVATE litedb_vindex)

add_test(NAME vector_index_tests COMMAND vector_index_tests)
