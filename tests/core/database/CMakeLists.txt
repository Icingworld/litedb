add_executable(database_tests
    database_tests.cpp
)

target_link_libraries(database_tests PRIVATE litedb_database)

add_test(NAME database_tests COMMAND database_tests)

add_executable(database_recovery_tests
    database_recovery_tests.cpp
)

target_link_libraries(database_recovery_tests PRIVATE litedb_database)

add_test(NAME database_recovery_tests COMMAND database_recovery_tests)
