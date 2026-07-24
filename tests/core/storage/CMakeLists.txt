add_executable(storage_engine_tests
    storage_engine_tests.cpp
)

target_link_libraries(storage_engine_tests PRIVATE litedb_storage)

add_test(NAME storage_engine_tests COMMAND storage_engine_tests)

add_executable(storage_store_tests
    storage_store_tests.cpp
)

target_link_libraries(storage_store_tests PRIVATE litedb_storage)

add_test(NAME storage_store_tests COMMAND storage_store_tests)

add_executable(storage_value_codec_tests
    value_codec_tests.cpp
)

target_link_libraries(storage_value_codec_tests PRIVATE litedb_storage)

add_test(NAME storage_value_codec_tests COMMAND storage_value_codec_tests)
