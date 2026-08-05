set(FUNCTION_SOURCES
    function_helper.cpp
    function_catalog.cpp
    scalar_function.cpp
    builtin/builtin_functions.cpp
    builtin/vector_functions.cpp
)

add_library(litedb_function ${FUNCTION_SOURCES})

target_include_directories(litedb_function PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(litedb_function PUBLIC litedb_common)
