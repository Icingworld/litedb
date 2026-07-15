add_executable(scalar_index_tests
    scalar_index_tests.cpp
)

target_link_libraries(scalar_index_tests PRIVATE litedb_index)

add_test(NAME scalar_index_tests COMMAND scalar_index_tests)

add_executable(index_engine_tests
    index_engine_tests.cpp
)

target_link_libraries(index_engine_tests PRIVATE litedb_index)

add_test(NAME index_engine_tests COMMAND index_engine_tests)

add_executable(btree_page_tests
    btree_page_tests.cpp
)

target_link_libraries(btree_page_tests PRIVATE litedb_index)

add_test(NAME btree_page_tests COMMAND btree_page_tests)

add_executable(btree_page_codec_tests
    btree_page_codec_tests.cpp
)

target_link_libraries(btree_page_codec_tests PRIVATE litedb_index)

add_test(NAME btree_page_codec_tests COMMAND btree_page_codec_tests)
