add_executable(physical_planner_tests
    physical_planner_tests.cpp
)

add_executable(physical_operator_lowering_tests
    physical_operator_lowering_tests.cpp
)

add_executable(physical_scalar_access_tests
    physical_scalar_access_tests.cpp
)

add_executable(physical_vector_top_k_tests
    physical_vector_top_k_tests.cpp
)

target_link_libraries(physical_planner_tests PRIVATE
    litedb_physical_planner
)

target_link_libraries(physical_operator_lowering_tests PRIVATE
    litedb_physical_planner
)

target_link_libraries(physical_scalar_access_tests PRIVATE
    litedb_physical_planner
)

target_link_libraries(physical_vector_top_k_tests PRIVATE
    litedb_physical_planner
)

add_test(NAME physical_planner_tests COMMAND physical_planner_tests)
add_test(NAME physical_operator_lowering_tests COMMAND physical_operator_lowering_tests)
add_test(NAME physical_scalar_access_tests COMMAND physical_scalar_access_tests)
add_test(NAME physical_vector_top_k_tests COMMAND physical_vector_top_k_tests)
