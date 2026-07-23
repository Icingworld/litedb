add_executable(error_tests
    error_tests.cpp
)

target_include_directories(error_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/internal/src
)

if(MSVC)
    target_compile_options(error_tests PRIVATE /GR- /EHs-c-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(error_tests PRIVATE -fno-rtti -fno-exceptions)
endif()

add_test(NAME error_tests COMMAND error_tests)
