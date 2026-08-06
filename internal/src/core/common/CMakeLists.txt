add_library(litedb_common
    identifier.cpp
    type_rules.cpp
    value.cpp
)

target_include_directories(litedb_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
