set(EXECUTOR_TEST_TARGETS
    executor_command_tests
    executor_operator_tests
    executor_query_tests
    executor_mutation_tests
)

foreach(TEST_TARGET IN LISTS EXECUTOR_TEST_TARGETS)
    add_executable(${TEST_TARGET}
        executor_tests.cpp
    )

    target_link_libraries(${TEST_TARGET} PRIVATE
        litedb_executor
        litedb_optimizer
    )

    if (TEST_TARGET STREQUAL executor_command_tests)
        target_compile_definitions(${TEST_TARGET} PRIVATE LITEDB_EXECUTOR_COMMAND_TESTS)
    elseif (TEST_TARGET STREQUAL executor_operator_tests)
        target_compile_definitions(${TEST_TARGET} PRIVATE LITEDB_EXECUTOR_OPERATOR_TESTS)
    elseif (TEST_TARGET STREQUAL executor_query_tests)
        target_compile_definitions(${TEST_TARGET} PRIVATE LITEDB_EXECUTOR_QUERY_TESTS)
    elseif (TEST_TARGET STREQUAL executor_mutation_tests)
        target_compile_definitions(${TEST_TARGET} PRIVATE LITEDB_EXECUTOR_MUTATION_TESTS)
    endif()

    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
endforeach()
