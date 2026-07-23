#include <cstdint>
#include <filesystem>
#include <string_view>
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

bool test_context_and_cause_chain()
{
    FileSystemErrorContext leaf_context {
        .operation = "open",
        .path = std::filesystem::path {"data.ldb"},
        .related_path = {},
        .native_code = std::make_error_code(std::errc::no_such_file_or_directory),
    };
    Error leaf {
        FileSystemErrorCode::NotFound,
        "database file does not exist",
        std::move(leaf_context),
    };

    Error root {
        FileSystemErrorCode::IoError,
        "failed to load database",
        OperationContext {.operation_id = 42},
        std::move(leaf),
    };

    if (root.category() != ErrorCategory::FileSystem ||
        root.code() != std::to_underlying(FileSystemErrorCode::IoError) ||
        root.message() != "failed to load database" ||
        root.encode_code() != ((static_cast<std::uint16_t>(ErrorCategory::FileSystem) << 8) |
                              std::to_underlying(FileSystemErrorCode::IoError))) {
        return false;
    }

    const auto * root_context = root.context<OperationContext>();
    if (root_context == nullptr || root_context->operation_id != 42 ||
        root.context<FileSystemErrorContext>() != nullptr) {
        return false;
    }

    const Error * cause = root.cause();
    if (cause == nullptr ||
        cause->code() != std::to_underlying(FileSystemErrorCode::NotFound) ||
        cause->message() != "database file does not exist" ||
        cause->cause() != nullptr) {
        return false;
    }

    const auto * cause_context = cause->context<FileSystemErrorContext>();
    return cause_context != nullptr &&
           cause_context->operation == "open" &&
           cause_context->path == std::filesystem::path {"data.ldb"} &&
           cause_context->related_path.empty() &&
           cause_context->native_code ==
               std::make_error_code(std::errc::no_such_file_or_directory) &&
           cause->context<OperationContext>() == nullptr;
}

bool test_move_preserves_context_and_cause()
{
    Error cause {
        FileSystemErrorCode::Unsupported,
        "operation is unsupported",
    };
    Error source {
        FileSystemErrorCode::IoError,
        "operation failed",
        OperationContext {.operation_id = 7},
        std::move(cause),
    };

    Error moved {std::move(source)};
    const auto * context = moved.context<OperationContext>();
    return context != nullptr &&
           context->operation_id == 7 &&
           moved.cause() != nullptr &&
           moved.cause()->code() ==
               std::to_underlying(FileSystemErrorCode::Unsupported);
}

bool test_cause_without_context()
{
    Error cause {
        FileSystemErrorCode::InvalidPath,
        "invalid source path",
    };
    Error root {
        FileSystemErrorCode::IoError,
        "rename failed",
        std::move(cause),
    };

    return root.context<OperationContext>() == nullptr &&
           root.cause() != nullptr &&
           root.cause()->code() ==
               std::to_underlying(FileSystemErrorCode::InvalidPath);
}

} // namespace

int main()
{
    return test_context_and_cause_chain() &&
                   test_move_preserves_context_and_cause() &&
                   test_cause_without_context()
               ? 0
               : 1;
}
