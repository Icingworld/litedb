add_executable(client_server_tests
    client_server_tests.cpp
)

target_link_libraries(client_server_tests PRIVATE
    litedb_client
    litedb_server
)

add_test(NAME client_server_tests COMMAND client_server_tests)
