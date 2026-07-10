add_executable(io_tests
    io_tests.cpp
)

target_link_libraries(io_tests PRIVATE litedb_io)

add_test(NAME io_tests COMMAND io_tests)
