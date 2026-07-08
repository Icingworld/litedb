add_executable(executor_tests
    executor_tests.cpp
)

target_link_libraries(executor_tests PRIVATE
    litedb_executor
    litedb_optimizer
)

add_test(NAME executor_tests COMMAND executor_tests)
