add_executable(physical_planner_tests
    physical_planner_tests.cpp
)

target_link_libraries(physical_planner_tests PRIVATE
    litedb_physical_planner
)

add_test(NAME physical_planner_tests COMMAND physical_planner_tests)
