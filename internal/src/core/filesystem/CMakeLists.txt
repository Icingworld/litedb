add_library(litedb_filesystem
    file_handle.cpp
    file_open_options.hpp
    filesystem.cpp
    platform_filesystem.cpp
)

# 根据不同平台添加不同的后端实现
if(WIN32)
    target_sources(litedb_filesystem PRIVATE
        backend/win32/win_file_handle_backend.cpp
        backend/win32/win_filesystem_backend.cpp
    )
elseif(UNIX)
    target_sources(litedb_filesystem PRIVATE
        backend/posix/posix_file_handle_backend.cpp
        backend/posix/posix_filesystem_backend.cpp
    )
else()
    message(FATAL_ERROR "LiteDB filesystem backend is not implemented for this platform")
endif()

target_include_directories(litedb_filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
