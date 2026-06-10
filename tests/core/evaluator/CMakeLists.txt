add_executable(evaluator_tests
    evaluator_tests.cpp
)

target_link_libraries(evaluator_tests PRIVATE litedb_evaluator)

add_test(NAME evaluator_tests COMMAND evaluator_tests)
