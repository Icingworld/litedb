foreach(FUNCTION_TEST_TARGET IN ITEMS
    function_catalog_tests
    function_binding_tests
    scalar_builtin_tests
)
    add_executable(${FUNCTION_TEST_TARGET}
        ${FUNCTION_TEST_TARGET}.cpp
    )
    target_link_libraries(${FUNCTION_TEST_TARGET} PRIVATE litedb_function)
    add_test(NAME ${FUNCTION_TEST_TARGET} COMMAND ${FUNCTION_TEST_TARGET})
endforeach()
