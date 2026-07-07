add_executable(optimizer_tests
    optimizer_tests.cpp
)

target_link_libraries(optimizer_tests PRIVATE
    litedb_optimizer
    litedb_engine
)

add_test(NAME optimizer_tests COMMAND optimizer_tests)
