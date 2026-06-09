add_executable(schema_tests
    schema_tests.cpp
)

target_link_libraries(schema_tests PRIVATE litedb_schema)

add_test(NAME schema_tests COMMAND schema_tests)
