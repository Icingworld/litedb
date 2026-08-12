#include <cstdint>
#include <filesystem>
#include <system_error>
#include <type_traits>
#include <utility>

#include "core/error/error.hpp"
#include "core/filesystem/filesystem_error.hpp"
#include "core/io/io_error.hpp"
#include "core/catalog/catalog_error.hpp"
#include "core/storage/storage_error.hpp"
#include "core/transaction/transaction_error.hpp"
#include "core/vindex/vector_index_error.hpp"

namespace
{

using litedb::core::error::Error;
using litedb::core::error::ErrorCategory;
using litedb::core::filesystem::FileSystemErrorCode;
using litedb::core::filesystem::FileSystemErrorContext;
using litedb::core::io::IoErrorCode;
using litedb::core::catalog::CatalogErrorCode;
using litedb::core::catalog::CatalogErrorContext;
using litedb::core::catalog::CatalogOperation;
using litedb::core::storage::StorageErrorCode;
using litedb::core::storage::StorageErrorContext;
using litedb::core::storage::StorageOperation;
using litedb::core::transaction::TransactionErrorCode;
using litedb::core::transaction::TransactionErrorContext;
using litedb::core::transaction::TransactionOperation;
using litedb::core::vindex::VectorIndexErrorCode;
using litedb::core::vindex::VectorIndexErrorContext;
using litedb::core::vindex::VectorIndexOperation;

static_assert(litedb::core::error::ErrorType<FileSystemErrorCode>);
static_assert(litedb::core::error::ErrorType<IoErrorCode>);
static_assert(litedb::core::error::ErrorType<CatalogErrorCode>);
static_assert(litedb::core::error::ErrorType<StorageErrorCode>);
static_assert(litedb::core::error::ErrorType<VectorIndexErrorCode>);
static_assert(std::to_underlying(ErrorCategory::FileSystem) == 1);
static_assert(std::to_underlying(ErrorCategory::Io) == 2);
static_assert(std::to_underlying(ErrorCategory::Catalog) == 3);
static_assert(std::to_underlying(ErrorCategory::Storage) == 4);
static_assert(std::to_underlying(ErrorCategory::VectorIndex) == 6);
static_assert(std::to_underlying(ErrorCategory::Transaction) == 8);
static_assert(!std::is_copy_constructible_v<Error>);
static_assert(!std::is_copy_assignable_v<Error>);
static_assert(std::is_nothrow_move_constructible_v<Error>);
static_assert(std::is_nothrow_move_assignable_v<Error>);

struct OperationContext
{
    std::uint64_t operation_id;
};

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
           stored != nullptr &&
           stored->operation == "open" &&
           stored->path == std::filesystem::path {"data.ldb"} &&
           stored->native_code == std::make_error_code(std::errc::no_such_file_or_directory) &&
           error.context<OperationContext>() == nullptr;
}

bool test_code_and_context()
{
    CatalogErrorContext context {
        .operation = CatalogOperation::Load,
        .path = std::filesystem::path {"catalog.lcat"},
        .source_code = static_cast<std::uint16_t>(0x0203),
    };
    Error error {CatalogErrorCode::IoFailure, "catalog read failed", std::move(context)};

    const auto * stored = error.context<CatalogErrorContext>();
    return error.category() == ErrorCategory::Catalog &&
           error.is(CatalogErrorCode::IoFailure) &&
           error.code() == std::to_underlying(CatalogErrorCode::IoFailure) &&
           error.encode_code() == 0x030D &&
           error.message() == "catalog read failed" &&
           stored != nullptr &&
           stored->operation == CatalogOperation::Load &&
           stored->path == std::filesystem::path {"catalog.lcat"} &&
           stored->source_code == 0x0203;
}

bool test_move_preserves_context()
{
    Error source {
        CatalogErrorCode::FileSystemFailure,
        "replace failed",
        CatalogErrorContext {
            .operation = CatalogOperation::PublishFile,
            .path = std::filesystem::path {"catalog.lcat"},
            .source_code = static_cast<std::uint16_t>(0x0104),
        },
    };
    Error moved {std::move(source)};
    const auto * context = moved.context<CatalogErrorContext>();
    return moved.is(CatalogErrorCode::FileSystemFailure) &&
           context != nullptr &&
           context->operation == CatalogOperation::PublishFile &&
           context->source_code == 0x0104;
}

bool test_storage_code_and_context()
{
    Error error {
        StorageErrorCode::ChecksumMismatch,
        "page checksum mismatch",
        StorageErrorContext {
            .operation = StorageOperation::ReadPage,
            .path = std::filesystem::path {"collections/7.store"},
            .collection_id = 7,
            .record_id = 9,
            .page_id = 3,
            .slot_id = 2,
            .source_code = static_cast<std::uint16_t>(0x0203),
        },
    };
    const auto * context = error.context<StorageErrorContext>();
    return error.category() == ErrorCategory::Storage &&
           error.is(StorageErrorCode::ChecksumMismatch) &&
           error.encode_code() == 0x0410 &&
           context != nullptr &&
           context->operation == StorageOperation::ReadPage &&
           context->collection_id == 7 &&
           context->record_id == 9 &&
           context->page_id == 3 &&
           context->slot_id == 2 &&
           context->source_code == 0x0203;
}

bool test_vector_index_code_and_context()
{
    Error error {
        VectorIndexErrorCode::ChecksumMismatch,
        "HNSW frame checksum mismatch",
        VectorIndexErrorContext {
            .operation = VectorIndexOperation::DecodeFrame,
            .index_id = 7,
            .collection_id = 9,
            .frame_sequence = 11,
            .path = std::filesystem::path {"vindexes/vindex_7.lhnsw"},
            .source_code = 0x010C,
        },
    };
    const auto * context = error.context<VectorIndexErrorContext>();
    return error.category() == ErrorCategory::VectorIndex &&
           error.is(VectorIndexErrorCode::ChecksumMismatch) &&
           error.encode_code() == 0x0615 &&
           context != nullptr &&
           context->operation == VectorIndexOperation::DecodeFrame &&
           context->index_id == 7 &&
           context->collection_id == 9 &&
           context->frame_sequence == 11 &&
           context->source_code == 0x010C;
}

bool test_transaction_error_preserves_cause()
{
    Error source {
        StorageErrorCode::ChecksumMismatch,
        "page checksum mismatch",
        StorageErrorContext {
            .operation = StorageOperation::ReadPage,
            .collection_id = 7,
            .page_id = 3,
        },
    };
    Error error {
        TransactionErrorCode::PrepareFailed,
        "storage transaction preparation failed",
        TransactionErrorContext {
            .operation = TransactionOperation::Prepare,
            .transaction_id = 11,
            .source_code = source.encode_code(),
        },
        std::move(source),
    };
    const auto * context = error.context<TransactionErrorContext>();
    return error.category() == ErrorCategory::Transaction &&
           error.is(TransactionErrorCode::PrepareFailed) &&
           context != nullptr &&
           context->operation == TransactionOperation::Prepare &&
           context->transaction_id == 11 &&
           context->source_code == 0x0410 &&
           error.cause() != nullptr &&
           error.cause()->is(StorageErrorCode::ChecksumMismatch);
}

} // namespace

int main()
{
    return test_filesystem_context() &&
           test_code_and_context() &&
           test_move_preserves_context() &&
           test_storage_code_and_context() &&
           test_vector_index_code_and_context() &&
           test_transaction_error_preserves_cause()
        ? 0
        : 1;
}
