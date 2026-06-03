add_executable(memory_page_cache_tests
    page_cache_tests.cpp
)

target_link_libraries(memory_page_cache_tests PRIVATE litedb_memory)

add_test(NAME memory_page_cache_tests COMMAND memory_page_cache_tests)

add_executable(memory_central_cache_tests
    central_cache_tests.cpp
)

target_link_libraries(memory_central_cache_tests PRIVATE litedb_memory)

add_test(NAME memory_central_cache_tests COMMAND memory_central_cache_tests)

add_executable(memory_thread_cache_tests
    thread_cache_tests.cpp
)

target_link_libraries(memory_thread_cache_tests PRIVATE litedb_memory)

add_test(NAME memory_thread_cache_tests COMMAND memory_thread_cache_tests)

add_executable(memory_pool_tests
    memory_pool_tests.cpp
)

target_link_libraries(memory_pool_tests PRIVATE litedb_memory)

add_test(NAME memory_pool_tests COMMAND memory_pool_tests)

add_executable(memory_resource_tests
    memory_resource_tests.cpp
)

target_link_libraries(memory_resource_tests PRIVATE litedb_memory)

add_test(NAME memory_resource_tests COMMAND memory_resource_tests)
