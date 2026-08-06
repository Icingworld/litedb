add_executable(logical_planner_tests
    logical_planner_tests.cpp
)

target_link_libraries(logical_planner_tests PRIVATE litedb_logical_planner)

add_test(NAME logical_planner_tests COMMAND logical_planner_tests)
