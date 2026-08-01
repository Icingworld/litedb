add_executable(optimizer_tests
    optimizer_tests.cpp
)

target_link_libraries(optimizer_tests PRIVATE
    litedb_optimizer
    litedb_filesystem
    litedb_executor
    litedb_index
    litedb_physical_planner
    litedb_storage
)

add_test(NAME optimizer_tests COMMAND optimizer_tests)
