add_executable(wal_tests wal_tests.cpp)
target_link_libraries(wal_tests PRIVATE litedb_wal litedb_filesystem)
add_test(NAME wal_tests COMMAND wal_tests)
