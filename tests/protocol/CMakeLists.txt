add_executable(protocol_tests
    protocol_tests.cpp
)

target_link_libraries(protocol_tests PRIVATE litedb_protocol)

add_test(NAME protocol_tests COMMAND protocol_tests)
