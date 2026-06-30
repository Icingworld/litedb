add_executable(binder_tests
    binder_tests.cpp
)

target_link_libraries(binder_tests PRIVATE litedb_binder)

add_test(NAME binder_tests COMMAND binder_tests)

add_executable(binder_debug_printer_tests
    debug_printer_tests.cpp
)

target_link_libraries(binder_debug_printer_tests PRIVATE litedb_binder)

add_test(NAME binder_debug_printer_tests COMMAND binder_debug_printer_tests)
