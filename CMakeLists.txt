cmake_minimum_required(VERSION 4.3.2)
project(litedb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/asio.cmake)

add_subdirectory(internal/src/memory)
add_subdirectory(internal/src/core/parser)
add_subdirectory(internal/src/core/catalog)
add_subdirectory(internal/src/core/schema)
add_subdirectory(internal/src/core/storage)
add_subdirectory(internal/src/core/binder)
add_subdirectory(internal/src/core/planner/logical)
add_subdirectory(internal/src/core/evaluator)
add_subdirectory(internal/src/core/executor)
add_subdirectory(internal/src/core/engine)
add_subdirectory(internal/src/protocol)
add_subdirectory(internal/src/net)
add_subdirectory(internal/src/server)
add_subdirectory(internal/src/client)

enable_testing()
add_subdirectory(tests)
