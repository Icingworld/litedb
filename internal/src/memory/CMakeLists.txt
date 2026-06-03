set(MEMORY_SOURCES
    common/free_list.cpp
    common/span_list.cpp
    common/platform.cpp
    common/size.cpp
    caches/page_cache.cpp
    caches/central_cache.cpp
    caches/thread_cache.cpp
    memory_pool.cpp
    memory_resources.cpp
)

add_library(litedb_memory ${MEMORY_SOURCES})

target_include_directories(litedb_memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
