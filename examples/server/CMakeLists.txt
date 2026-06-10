add_executable(litedb_example_server
    main.cpp
)

target_link_libraries(litedb_example_server PRIVATE
    litedb_server
)
