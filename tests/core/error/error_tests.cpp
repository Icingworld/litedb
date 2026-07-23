#include <cstdint>
#include <filesystem>
#include <system_error>
#include <type_traits>
#include <utility>

#include "core/error/error.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace
{

using litedb::core::error::Error;
using litedb::core::error::ErrorCategory;
using litedb::core::filesystem::FileSystemErrorCode;
using litedb::core::filesystem::FileSystemErrorContext;

struct OperationContext
{
    std::uint64_t operation_id;
};

static_assert(litedb::core::error::ErrorType<FileSystemErrorCode>);
static_assert(!std::is_copy_constructible_v<Error>);
static_assert(!std::is_copy_assignable_v<Error>);
static_assert(std::is_nothrow_move_constructible_v<Error>);
static_assert(std::is_nothrow_move_assignable_v<Error>);
static_assert(!std::is_polymorphic_v<FileSystemErrorContext>);

bool test_filesystem_context()
{
    FileSystemErrorContext context {
        .operation = "open",
        .path = std::filesystem::path {"data.ldb"},
        .related_path = {},
        .native_code = std::make_error_code(std::errc::no_such_file_or_directory),
    };
    Error error {
        FileSystemErrorCode::NotFound,
        "database file does not exist",
        std::move(context),
    };

    const auto * stored = error.context<FileSystemErrorContext>();
    return error.category() == ErrorCategory::FileSystem &&
           error.is(FileSystemErrorCode::NotFound) &&
           error.message() == "database file does not exist" &&
           error.encode_code() ==
               ((static_cast<std::uint16_t>(ErrorCategory::FileSystem) << 8) |
                std::to_underlying(FileSystemErrorCode::NotFound)) &&
           stored != nullptr &&
           stored->operation == "open" &&
           stored->path == std::filesystem::path {"data.ldb"} &&
           stored->native_code ==
               std::make_error_code(std::errc::no_such_file_or_directory) &&
           error.context<OperationContext>() == nullptr;
}

bool test_move_preserves_context()
{
    Error source {
        FileSystemErrorCode::IoError,
        "operation failed",
        OperationContext {.operation_id = 7},
    };
    Error moved {std::move(source)};
    const auto * context = moved.context<OperationContext>();
    return moved.is(FileSystemErrorCode::IoError) &&
           context != nullptr &&
           context->operation_id == 7;
}

bool test_error_without_context()
{
    Error error {
        FileSystemErrorCode::Unsupported,
        "operation is unsupported",
    };
    return error.is(FileSystemErrorCode::Unsupported) &&
           error.context<OperationContext>() == nullptr &&
           error.context<FileSystemErrorContext>() == nullptr;
}

} // namespace

int main()
{
    return test_filesystem_context() &&
                   test_move_preserves_context() &&
                   test_error_without_context()
               ? 0
               : 1;
}
