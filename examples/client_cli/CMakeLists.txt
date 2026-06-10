add_executable(litedb_example_client_cli
    main.cpp
)

target_link_libraries(litedb_example_client_cli PRIVATE
    litedb_client
)
