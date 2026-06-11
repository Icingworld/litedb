add_executable(persistence_tests
    persistence_tests.cpp
)

target_link_libraries(persistence_tests PRIVATE
    litedb_engine
    litedb_persistence
)

add_test(NAME persistence_tests COMMAND persistence_tests)
