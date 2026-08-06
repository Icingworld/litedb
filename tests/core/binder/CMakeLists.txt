foreach(BINDER_TEST_TARGET IN ITEMS
    binder_expression_tests
    binder_query_tests
    binder_mutation_tests
    binder_ddl_tests
)
    add_executable(${BINDER_TEST_TARGET}
        ${BINDER_TEST_TARGET}.cpp
    )
    target_link_libraries(${BINDER_TEST_TARGET} PRIVATE litedb_binder)
    add_test(NAME ${BINDER_TEST_TARGET} COMMAND ${BINDER_TEST_TARGET})
endforeach()

add_executable(binder_debug_printer_tests
    debug_printer_tests.cpp
)

target_link_libraries(binder_debug_printer_tests PRIVATE litedb_binder)

add_test(NAME binder_debug_printer_tests COMMAND binder_debug_printer_tests)
