add_library(litedb_transaction
    transaction_context.cpp
    transaction_file_overlay.cpp
    transaction_manager.cpp
    transaction_state.cpp
)
target_include_directories(litedb_transaction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(litedb_transaction PUBLIC
    litedb_schema
    litedb_wal
    litedb_catalog
    litedb_storage
    litedb_index
    litedb_vindex
)
