add_executable(physical_plan_tests
    physical_plan_tests.cpp
)

target_link_libraries(physical_plan_tests PRIVATE
    litedb_physical_plan
)

add_test(NAME physical_plan_tests COMMAND physical_plan_tests)
