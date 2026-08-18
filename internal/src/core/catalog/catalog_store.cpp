#include "core/catalog/catalog_store.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <format>
#include <limits>

#include "catalog_snapshot.hpp"
#include "core/catalog/catalog_constant.hpp"
#include "core/catalog/catalog_snapshot.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/checksum.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/schema/default_expression.hpp"

namespace litedb::core::catalog
{

namespace
{

// 下一个序列 ID，用于防止并发时，文件名冲突
std::atomic<std::uint64_t> next_sequence_id = 1;

// 临时文件清理器
// 用于在对象生命周期结束时，自动清理临时文件
// 如果在临时文件操作过程中出错，则会自动删除
class TempFileCleanup
{
public:
    TempFileCleanup(filesystem::FileSystem & filesystem, std::filesystem::path path)
        : filesystem_(&filesystem)
        , path_(std::move(path))
    {}

    ~TempFileCleanup()
    {
        if (active_) {
            // 尽力删除临时文件，如果出错，无能为力
            static_cast<void>(filesystem_->remove(path_));
        }
    }

public:
    // 取消自动删除临时文件
    void release() noexcept
    {
        active_ = false;
    }

private:
    filesystem::FileSystem * filesystem_;
    std::filesystem::path path_;
    bool active_ {true};
};

// 读取默认表达式
std::expected<schema::DefaultExpression, CatalogError> read_default_expression(
    io::LittleEndianBinaryReader & reader,
    const std::filesystem::path & path,
    std::size_t depth = 0
)
{
    if (depth >= MaxExpressionDepth) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "default expression nesting exceeds limit",
            {
                .operation = CatalogOperation::Decode,
                .path = path,
            }
        ));
    }

    // 读取默认表达式
    schema::DefaultExpression expression;

    auto expression_kind = reader.read_u8();
    if (!expression_kind) [[unlikely]] {
        return std::unexpected(std::move(expression_kind.error()));
    }
    if (*expression_kind > static_cast<std::uint8_t>(schema::DefaultExpressionKind::Vector))
        [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "invalid default expression kind",
            {
                .operation = CatalogOperation::Decode,
                .path = path,
            }
        ));
    }
    expression.kind = static_cast<schema::DefaultExpressionKind>(*expression_kind);

    auto literal_kind = reader.read_u8();
    if (!literal_kind) [[unlikely]] {
        return std::unexpected(std::move(literal_kind.error()));
    }
    if (*literal_kind > static_cast<std::uint8_t>(schema::DefaultLiteralKind::String))
        [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "invalid default literal kind",
            {
                .operation = CatalogOperation::Decode,
                .path = path,
            }
        ));
    }
    expression.literal_kind = static_cast<schema::DefaultLiteralKind>(*literal_kind);

    auto value = reader.read_string();
    if (!value) [[unlikely]] {
        return std::unexpected(std::move(value.error()));
    }
    expression.value = *value;

    auto element_count = reader.read_u32();
    if (!element_count) [[unlikely]] {
        return std::unexpected(std::move(element_count.error()));
    }
    // 非向量类型，元素数量必须为0
    if (*element_count > 0 &&
        *expression_kind == static_cast<std::uint8_t>(schema::DefaultExpressionKind::Literal)) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "default expression element count is not zero when expression kind is not vector",
            {
                .operation = CatalogOperation::Decode,
                .path = path,
            }
        ));
    }
    if (*element_count > MaxEntryCount) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "default expression element count exceeds limit",
            {
                .operation = CatalogOperation::Decode,
                .path = path,
            }
        ));
    }

    // 根据剩余预算，估计一下元素数量是否合法
    const auto element_remaining_bytes = reader.remaining_bytes();
    // 最小大小的结构为：
    // DefaultExpressionKind kind: 1 bytes
    // DefaultLiteralKind literal_kind: 1 bytes
    // std::uint32_t value_size: 4 bytes
    // std::uint32_t element_count: 4 bytes
    // 最小大小为 10 bytes
    constexpr std::size_t min_element_bytes =
        sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint32_t) + sizeof(std::uint32_t);
    auto max_element_count = element_remaining_bytes / min_element_bytes;
    if (*element_count > max_element_count) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "default expression element count exceeds limit",
            {
                .operation = CatalogOperation::Decode,
                .path = path,
            }
        ));
    }

    expression.elements.reserve(*element_count);

    // 递归读取元素表达式
    for (std::uint32_t i = 0; i < *element_count; ++i) {
        auto element = read_default_expression(reader, path, depth + 1);
        if (!element) [[unlikely]] {
            return std::unexpected(std::move(element.error()));
        }
        expression.elements.push_back(std::move(*element));
    }

    return expression;
}

// 写入默认表达式
std::expected<void, CatalogError> write_default_expression(
    io::LittleEndianBinaryWriter & writer,
    const schema::DefaultExpression & expression,
    const std::filesystem::path & path,
    std::size_t depth = 0
)
{
    if (depth >= MaxExpressionDepth) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "default expression nesting exceeds limit",
            {
                .operation = CatalogOperation::Encode,
                .path = path,
            }
        ));
    }

    auto expression_kind = writer.write_u8(static_cast<std::uint8_t>(expression.kind));
    if (!expression_kind) [[unlikely]] {
        return std::unexpected(std::move(expression_kind.error()));
    }

    auto literal_kind = writer.write_u8(static_cast<std::uint8_t>(expression.literal_kind));
    if (!literal_kind) [[unlikely]] {
        return std::unexpected(std::move(literal_kind.error()));
    }

    if (expression.value.size() > MaxStringSize) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "default expression value exceeds limit",
            {
                .operation = CatalogOperation::Encode,
                .path = path,
            }
        ));
    }
    auto value = writer.write_string(expression.value);
    if (!value) [[unlikely]] {
        return std::unexpected(std::move(value.error()));
    }

    if (expression.elements.size() > MaxEntryCount) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "default expression element count exceeds limit",
            {
                .operation = CatalogOperation::Encode,
                .path = path,
            }
        ));
    }
    auto element_count_size = static_cast<std::uint32_t>(expression.elements.size());
    auto element_count = writer.write_u32(element_count_size);
    if (!element_count) [[unlikely]] {
        return std::unexpected(std::move(element_count.error()));
    }

    for (const auto & element : expression.elements) {
        auto element_encoded = write_default_expression(writer, element, path, depth + 1);
        if (!element_encoded) [[unlikely]] {
            return std::unexpected(std::move(element_encoded.error()));
        }
    }
    return {};
}

} // namespace

CatalogStore::CatalogStore(std::filesystem::path path, filesystem::FileSystem & filesystem)
    : path_(std::move(path))
    , filesystem_(&filesystem)
{
    assert(!path_.empty());
}

std::expected<std::optional<CatalogSnapshot>, CatalogError> CatalogStore::load() const
{
    // 加载目录文件
    auto exists = filesystem_->exists(path_);
    if (!exists) [[unlikely]] {
        return std::unexpected(std::move(exists.error()));
    }
    if (!*exists) [[unlikely]] {
        return std::optional<CatalogSnapshot> {};
    }

    // 打开目录文件
    auto file = filesystem_->open(
        path_,
        {.access = filesystem::FileAccess::ReadOnly,
         .create_mode = filesystem::FileCreateMode::OpenExisting}
    );
    if (!file) [[unlikely]] {
        return std::unexpected(std::move(file.error()));
    }

    // 验证目录文件是否合法
    auto file_size = file->size();
    if (!file_size) [[unlikely]] {
        return std::unexpected(std::move(file_size.error()));
    }
    if (*file_size < CatalogHeaderSize) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::UnexpectedEof,
            "catalog file header is truncated",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }
    if (*file_size > MaxPayloadSize + CatalogHeaderSize) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "catalog file exceeds size limit",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    io::FileByteReader file_reader {*file};

    // 读取目录文件头
    std::array<std::byte, CatalogHeaderSize> header_bytes {};
    if (auto read = file_reader.read_exact(header_bytes); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }

    io::BufferByteReader header_resource {std::span<const std::byte> {header_bytes}};
    io::LittleEndianBinaryReader header_reader {
        header_resource,
        {
            .max_total_bytes = CatalogHeaderSize,
            .max_string_bytes = 0,
        }
    };

    // 读取并验证魔数
    auto magic = header_reader.read_u32();
    if (!magic) [[unlikely]] {
        return std::unexpected(std::move(magic.error()));
    }
    if (*magic != CatalogMagic) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "catalog file has invalid magic number",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    // 读取并验证版本号
    auto version = header_reader.read_u16();
    if (!version) [[unlikely]] {
        return std::unexpected(std::move(version.error()));
    }
    if (*version != CatalogVersion) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::UnsupportedVersion,
            "catalog file has unsupported version number",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    // 读取并验证头部长度
    auto header_length = header_reader.read_u16();
    if (!header_length) [[unlikely]] {
        return std::unexpected(std::move(header_length.error()));
    }
    if (*header_length != CatalogHeaderSize) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "catalog file has invalid header length",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    // 读取并验证负载大小
    auto payload_size = header_reader.read_u64();
    if (!payload_size) [[unlikely]] {
        return std::unexpected(std::move(payload_size.error()));
    }
    if (*payload_size > MaxPayloadSize || *payload_size != *file_size - CatalogHeaderSize)
        [[unlikely]] {
        return std::unexpected(make_error(
            *payload_size > MaxPayloadSize ? CatalogErrorCode::ResourceLimitExceeded
                                           : CatalogErrorCode::InvalidFormat,
            "invalid catalog payload size",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    // 读取负载校验和，等到读取负载数据后再验证
    auto payload_checksum = header_reader.read_u32();
    if (!payload_checksum) [[unlikely]] {
        return std::unexpected(std::move(payload_checksum.error()));
    }

    // 读取并验证标志
    // 目前没有添加标志行为
    auto flags = header_reader.read_u32();
    if (!flags) [[unlikely]] {
        return std::unexpected(std::move(flags.error()));
    }
    if (*flags != 0) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "invalid catalog flags",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    // 读取负载数据
    std::vector<std::byte> payload_bytes(static_cast<std::size_t>(*payload_size));
    if (auto read = file_reader.read_exact(payload_bytes); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    // 验证校验和
    if (io::crc32(payload_bytes) != *payload_checksum) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ChecksumMismatch,
            "catalog payload checksum mismatch",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    io::BufferByteReader payload_resource {std::span<const std::byte> {payload_bytes}};
    io::LittleEndianBinaryReader payload_reader {
        payload_resource,
        {.max_total_bytes = *payload_size, .max_string_bytes = MaxStringSize},
    };

    // 从负载数据中读取元数据快照
    CatalogSnapshot snapshot;

    auto next_database_id = payload_reader.read_u64();
    if (!next_database_id) [[unlikely]] {
        return std::unexpected(std::move(next_database_id.error()));
    }
    snapshot.next_database_id = *next_database_id;

    auto next_collection_id = payload_reader.read_u64();
    if (!next_collection_id) [[unlikely]] {
        return std::unexpected(std::move(next_collection_id.error()));
    }
    snapshot.next_collection_id = *next_collection_id;

    auto next_column_id = payload_reader.read_u64();
    if (!next_column_id) [[unlikely]] {
        return std::unexpected(std::move(next_column_id.error()));
    }
    snapshot.next_column_id = *next_column_id;

    auto next_index_id = payload_reader.read_u64();
    if (!next_index_id) [[unlikely]] {
        return std::unexpected(std::move(next_index_id.error()));
    }
    snapshot.next_index_id = *next_index_id;

    auto next_vector_index_id = payload_reader.read_u64();
    if (!next_vector_index_id) [[unlikely]] {
        return std::unexpected(std::move(next_vector_index_id.error()));
    }
    snapshot.next_vector_index_id = *next_vector_index_id;

    // 读取数据库数量
    auto database_count = payload_reader.read_u32();
    if (!database_count) [[unlikely]] {
        return std::unexpected(std::move(database_count.error()));
    }
    if (*database_count > MaxEntryCount) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "catalog database count exceeds limit",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    // 根据剩余预算，估计一下数据库数量是否合法
    const auto database_remaining_bytes = payload_reader.remaining_bytes();
    // 最小大小的结构为：
    // common::DatabaseId database_id: 8 bytes
    // 空 std::uint32_t name_size: 4 bytes
    // std::uint32_t collection_count: 4 bytes
    // 最小大小为 16 bytes
    constexpr std::size_t min_database_bytes =
        sizeof(std::uint64_t) + sizeof(std::uint32_t) + sizeof(std::uint32_t);
    auto max_database_count = database_remaining_bytes / min_database_bytes;
    if (*database_count > max_database_count) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ResourceLimitExceeded,
            "catalog database count exceeds limit",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    snapshot.databases.reserve(*database_count);

    // 读取每个数据库快照
    for (std::uint32_t database_index = 0; database_index < *database_count; ++database_index) {
        CatalogDatabaseSnapshot database;

        auto database_id = payload_reader.read_u64();
        if (!database_id) [[unlikely]] {
            return std::unexpected(std::move(database_id.error()));
        }
        database.id = *database_id;

        auto name = payload_reader.read_string();
        if (!name) [[unlikely]] {
            return std::unexpected(std::move(name.error()));
        }
        database.name = *name;

        // 读取集合数量
        auto collection_count = payload_reader.read_u32();
        if (!collection_count) [[unlikely]] {
            return std::unexpected(std::move(collection_count.error()));
        }
        if (*collection_count > MaxEntryCount) [[unlikely]] {
            return std::unexpected(make_error(
                CatalogErrorCode::ResourceLimitExceeded,
                "catalog collection count exceeds limit",
                {
                    .operation = CatalogOperation::Decode,
                    .path = path_,
                }
            ));
        }

        // 根据剩余预算，估计一下集合数量是否合法
        const auto collection_remaining_bytes = payload_reader.remaining_bytes();
        // 最小大小的结构为：
        // common::CollectionId collection_id: 8 bytes
        // common::DatabaseId database_id: 8 bytes
        // std::uint32_t name_size: 4 bytes
        // std::uint8_t comment_present: 1 byte
        // std::uint32_t column_count: 4 bytes
        // std::uint32_t index_count: 4 bytes
        // std::uint32_t vector_index_count: 4 bytes
        // 最小大小为 33 bytes
        constexpr std::size_t min_collection_bytes = sizeof(std::uint64_t) + sizeof(std::uint64_t) +
                                                     sizeof(std::uint32_t) + sizeof(std::uint8_t) +
                                                     sizeof(std::uint32_t) + sizeof(std::uint32_t) +
                                                     sizeof(std::uint32_t);
        auto max_collection_count = collection_remaining_bytes / min_collection_bytes;
        if (*collection_count > max_collection_count) [[unlikely]] {
            return std::unexpected(make_error(
                CatalogErrorCode::ResourceLimitExceeded,
                "catalog collection count exceeds limit",
                {
                    .operation = CatalogOperation::Decode,
                    .path = path_,
                }
            ));
        }

        database.collections.reserve(*collection_count);

        // 读取每个集合快照
        for (std::uint32_t collection_index = 0; collection_index < *collection_count;
             ++collection_index) {
            CatalogCollectionSnapshot collection;

            auto collection_id = payload_reader.read_u64();
            if (!collection_id) [[unlikely]] {
                return std::unexpected(std::move(collection_id.error()));
            }
            collection.id = *collection_id;

            auto database_id = payload_reader.read_u64();
            if (!database_id) [[unlikely]] {
                return std::unexpected(std::move(database_id.error()));
            }
            collection.database_id = *database_id;

            auto name = payload_reader.read_string();
            if (!name) [[unlikely]] {
                return std::unexpected(std::move(name.error()));
            }
            collection.name = *name;

            auto comment_present = payload_reader.read_u8();
            if (!comment_present) [[unlikely]] {
                return std::unexpected(std::move(comment_present.error()));
            }
            if (*comment_present > 1) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::InvalidFormat,
                    "invalid comment present value",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }
            if (*comment_present == 1) {
                auto comment = payload_reader.read_string();
                if (!comment) [[unlikely]] {
                    return std::unexpected(std::move(comment.error()));
                }
                collection.comment = *comment;
            }

            // 读取列数量
            auto column_count = payload_reader.read_u32();
            if (!column_count) [[unlikely]] {
                return std::unexpected(std::move(column_count.error()));
            }
            if (*column_count > MaxEntryCount) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "catalog column count exceeds limit",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }

            // 根据剩余预算，估计一下列数量是否合法
            const auto column_remaining_bytes = payload_reader.remaining_bytes();
            // 最小大小的结构为：
            // common::ColumnId column_id: 8 bytes
            // std::uint32_t name_size: 4 bytes
            // std::uint8_t type_id: 1 byte
            // std::uint8_t type_parameter_present: 1 byte
            // std::uint8_t unique: 1 byte
            // std::uint8_t nullable: 1 byte
            // std::uint8_t default_expression_present: 1 byte
            // std::uint8_t comment_present: 1 byte
            // 最小大小为 18 bytes
            constexpr std::size_t min_column_bytes = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                                     sizeof(std::uint8_t) + sizeof(std::uint8_t) +
                                                     sizeof(std::uint8_t) + sizeof(std::uint8_t) +
                                                     sizeof(std::uint8_t) + sizeof(std::uint8_t);
            auto max_column_count = column_remaining_bytes / min_column_bytes;
            if (*column_count > max_column_count) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "catalog column count exceeds limit",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }

            collection.columns.reserve(*column_count);

            // 读取每个列快照
            for (std::uint32_t column_index = 0; column_index < *column_count; ++column_index) {
                CatalogColumnSnapshot column;

                auto column_id = payload_reader.read_u64();
                if (!column_id) [[unlikely]] {
                    return std::unexpected(std::move(column_id.error()));
                }
                column.id = *column_id;

                auto name = payload_reader.read_string();
                if (!name) [[unlikely]] {
                    return std::unexpected(std::move(name.error()));
                }
                column.name = *name;

                auto type_id = payload_reader.read_u8();
                if (!type_id) [[unlikely]] {
                    return std::unexpected(std::move(type_id.error()));
                }
                // 类型验证依赖当前的枚举实现
                if (*type_id > static_cast<std::uint8_t>(common::LogicalTypeId::Vector))
                    [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid logical type id",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                column.type.id = static_cast<common::LogicalTypeId>(*type_id);

                auto type_parameter_present = payload_reader.read_u8();
                if (!type_parameter_present) [[unlikely]] {
                    return std::unexpected(std::move(type_parameter_present.error()));
                }
                if (*type_parameter_present > 1) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid type parameter present value",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                if (*type_parameter_present == 1) {
                    auto type_parameter = payload_reader.read_u64();
                    if (!type_parameter) [[unlikely]] {
                        return std::unexpected(std::move(type_parameter.error()));
                    }
                    if (*type_parameter > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
                        return std::unexpected(make_error(
                            CatalogErrorCode::ResourceLimitExceeded,
                            "size does not fit this platform",
                            {
                                .operation = CatalogOperation::Decode,
                                .path = path_,
                            }
                        ));
                    }
                    column.type.parameter = static_cast<std::size_t>(*type_parameter);
                }

                auto unique = payload_reader.read_u8();
                if (!unique) [[unlikely]] {
                    return std::unexpected(std::move(unique.error()));
                }
                if (*unique > 1) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid unique value",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                column.unique = (*unique == 1);

                auto nullable = payload_reader.read_u8();
                if (!nullable) [[unlikely]] {
                    return std::unexpected(std::move(nullable.error()));
                }
                if (*nullable > 1) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid nullable value",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                column.nullable = (*nullable == 1);

                auto default_expression_present = payload_reader.read_u8();
                if (!default_expression_present) [[unlikely]] {
                    return std::unexpected(std::move(default_expression_present.error()));
                }
                if (*default_expression_present > 1) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid default expression present value",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                if (*default_expression_present == 1) {
                    auto default_expression = read_default_expression(payload_reader, path_, 0);
                    if (!default_expression) [[unlikely]] {
                        return std::unexpected(std::move(default_expression.error()));
                    }
                    column.default_expression = std::move(*default_expression);
                }

                auto comment_present = payload_reader.read_u8();
                if (!comment_present) [[unlikely]] {
                    return std::unexpected(std::move(comment_present.error()));
                }
                if (*comment_present > 1) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid comment present value",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                if (*comment_present == 1) {
                    auto comment = payload_reader.read_string();
                    if (!comment) [[unlikely]] {
                        return std::unexpected(std::move(comment.error()));
                    }
                    column.comment = *comment;
                }

                collection.columns.push_back(std::move(column));
            }

            // 读取索引数量
            auto index_count = payload_reader.read_u32();
            if (!index_count) [[unlikely]] {
                return std::unexpected(std::move(index_count.error()));
            }
            if (*index_count > MaxEntryCount) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "catalog index count exceeds limit",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }

            // 根据剩余预算，估计一下索引数量是否合法
            const auto index_remaining_bytes = payload_reader.remaining_bytes();
            // 最小大小的结构为：
            // common::IndexId index_id: 8 bytes
            // common::ColumnId column_id: 8 bytes
            // std::uint32_t name_size: 4 bytes
            // std::uint8_t index_kind: 1 byte
            // std::uint8_t unique: 1 byte
            // 最小大小为 22 bytes
            constexpr std::size_t min_index_bytes = sizeof(std::uint64_t) + sizeof(std::uint64_t) +
                                                    sizeof(std::uint32_t) + sizeof(std::uint8_t) +
                                                    sizeof(std::uint8_t);
            auto max_index_count = index_remaining_bytes / min_index_bytes;
            if (*index_count > max_index_count) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "catalog index count exceeds limit",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }

            collection.indexes.reserve(*index_count);

            // 读取每个索引快照
            for (std::uint32_t index_index = 0; index_index < *index_count; ++index_index) {
                CatalogIndexSnapshot index;

                auto index_id = payload_reader.read_u64();
                if (!index_id) [[unlikely]] {
                    return std::unexpected(std::move(index_id.error()));
                }
                index.id = *index_id;

                auto column_id = payload_reader.read_u64();
                if (!column_id) [[unlikely]] {
                    return std::unexpected(std::move(column_id.error()));
                }
                index.column_id = *column_id;

                auto name = payload_reader.read_string();
                if (!name) [[unlikely]] {
                    return std::unexpected(std::move(name.error()));
                }
                index.name = *name;

                auto index_kind = payload_reader.read_u8();
                if (!index_kind) [[unlikely]] {
                    return std::unexpected(std::move(index_kind.error()));
                }
                // 目前只支持 BTree 索引，如果添加了索引实现，这里也需要修改
                if (*index_kind > static_cast<std::uint8_t>(entry::IndexKind::BTree)) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid index kind",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                index.index_kind = static_cast<entry::IndexKind>(*index_kind);

                auto unique = payload_reader.read_u8();
                if (!unique) [[unlikely]] {
                    return std::unexpected(std::move(unique.error()));
                }
                if (*unique > 1) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid unique value",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                index.unique = (*unique == 1);

                collection.indexes.push_back(std::move(index));
            }

            // 读取向量索引数量
            auto vector_index_count = payload_reader.read_u32();
            if (!vector_index_count) [[unlikely]] {
                return std::unexpected(std::move(vector_index_count.error()));
            }
            if (*vector_index_count > MaxEntryCount) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "catalog vector index count exceeds limit",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }

            // 根据剩余预算，估计一下向量索引数量是否合法
            const auto vector_index_remaining_bytes = payload_reader.remaining_bytes();
            // 最小大小的结构为：
            // common::VIndexId vector_index_id: 8 bytes
            // common::ColumnId column_id: 8 bytes
            // std::uint32_t name_size: 4 bytes
            // std::uint8_t index_kind: 1 byte
            // std::uint8_t metric: 1 byte
            // std::uint64_t dimension: 8 bytes
            // std::uint64_t max_neighbors: 8 bytes
            // std::uint64_t ef_construction: 8 bytes
            // std::uint64_t ef_search_default: 8 bytes
            // std::uint64_t random_seed: 8 bytes
            // 最小大小为 62 bytes
            constexpr std::size_t min_vector_index_bytes =
                sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint64_t) +
                sizeof(std::uint64_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t) +
                sizeof(std::uint64_t);
            auto max_vector_index_count = vector_index_remaining_bytes / min_vector_index_bytes;
            if (*vector_index_count > max_vector_index_count) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "catalog vector index count exceeds limit",
                    {
                        .operation = CatalogOperation::Decode,
                        .path = path_,
                    }
                ));
            }

            collection.vector_indexes.reserve(*vector_index_count);

            // 读取每个向量索引快照
            for (std::uint32_t vector_index_index = 0; vector_index_index < *vector_index_count;
                 ++vector_index_index) {
                CatalogVectorIndexSnapshot vector_index;

                auto vector_index_id = payload_reader.read_u64();
                if (!vector_index_id) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_id.error()));
                }
                vector_index.id = *vector_index_id;

                auto column_id = payload_reader.read_u64();
                if (!column_id) [[unlikely]] {
                    return std::unexpected(std::move(column_id.error()));
                }
                vector_index.column_id = *column_id;

                auto name = payload_reader.read_string();
                if (!name) [[unlikely]] {
                    return std::unexpected(std::move(name.error()));
                }
                vector_index.name = *name;

                auto index_kind = payload_reader.read_u8();
                if (!index_kind) [[unlikely]] {
                    return std::unexpected(std::move(index_kind.error()));
                }
                // 目前只支持 HNSW 向量索引，如果添加了向量索引实现，这里也需要修改
                if (*index_kind > static_cast<std::uint8_t>(entry::VectorIndexKind::Hnsw))
                    [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid vector index kind",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.index_kind = static_cast<entry::VectorIndexKind>(*index_kind);

                auto metric = payload_reader.read_u8();
                if (!metric) [[unlikely]] {
                    return std::unexpected(std::move(metric.error()));
                }
                // 目前只支持 L2、内积和余弦距离，如果添加了向量距离度量，这里也需要修改
                if (*metric > static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine))
                    [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidFormat,
                        "invalid vector distance metric",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.metric = static_cast<entry::VectorDistanceMetric>(*metric);

                auto dimension = payload_reader.read_u64();
                if (!dimension) [[unlikely]] {
                    return std::unexpected(std::move(dimension.error()));
                }
                if (*dimension > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "size does not fit this platform",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.dimension = static_cast<std::size_t>(*dimension);

                auto max_neighbors = payload_reader.read_u64();
                if (!max_neighbors) [[unlikely]] {
                    return std::unexpected(std::move(max_neighbors.error()));
                }
                if (*max_neighbors > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "size does not fit this platform",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.max_neighbors = static_cast<std::size_t>(*max_neighbors);

                auto ef_construction = payload_reader.read_u64();
                if (!ef_construction) [[unlikely]] {
                    return std::unexpected(std::move(ef_construction.error()));
                }
                if (*ef_construction > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "size does not fit this platform",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.ef_construction = static_cast<std::size_t>(*ef_construction);

                auto ef_search_default = payload_reader.read_u64();
                if (!ef_search_default) [[unlikely]] {
                    return std::unexpected(std::move(ef_search_default.error()));
                }
                if (*ef_search_default > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "size does not fit this platform",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.ef_search_default = static_cast<std::size_t>(*ef_search_default);

                auto random_seed = payload_reader.read_u64();
                if (!random_seed) [[unlikely]] {
                    return std::unexpected(std::move(random_seed.error()));
                }
                if (*random_seed > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "size does not fit this platform",
                        {
                            .operation = CatalogOperation::Decode,
                            .path = path_,
                        }
                    ));
                }
                vector_index.random_seed = static_cast<std::size_t>(*random_seed);

                collection.vector_indexes.push_back(std::move(vector_index));
            }

            database.collections.push_back(std::move(collection));
        }

        snapshot.databases.push_back(std::move(database));
    }

    // 如果解析完后依然有剩余数据，由于此前已经验证过校验码
    // 说明该文件本身不合法，可能被篡改或已经损坏
    if (payload_reader.remaining_bytes() != 0) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidFormat,
            "payload contains trailing bytes",
            {
                .operation = CatalogOperation::Decode,
                .path = path_,
            }
        ));
    }

    return snapshot;
}

std::expected<void, CatalogError> CatalogStore::save(const CatalogSnapshot & snapshot) const
{
    io::BufferByteWriter payload_bytes {MaxPayloadSize};
    io::LittleEndianBinaryWriter payload_writer {payload_bytes};

    // 写入负载数据

    auto next_database_id = payload_writer.write_u64(snapshot.next_database_id);
    if (!next_database_id) [[unlikely]] {
        return std::unexpected(std::move(next_database_id.error()));
    }

    auto next_collection_id = payload_writer.write_u64(snapshot.next_collection_id);
    if (!next_collection_id) [[unlikely]] {
        return std::unexpected(std::move(next_collection_id.error()));
    }

    auto next_column_id = payload_writer.write_u64(snapshot.next_column_id);
    if (!next_column_id) [[unlikely]] {
        return std::unexpected(std::move(next_column_id.error()));
    }

    auto next_index_id = payload_writer.write_u64(snapshot.next_index_id);
    if (!next_index_id) [[unlikely]] {
        return std::unexpected(std::move(next_index_id.error()));
    }

    auto next_vector_index_id = payload_writer.write_u64(snapshot.next_vector_index_id);
    if (!next_vector_index_id) [[unlikely]] {
        return std::unexpected(std::move(next_vector_index_id.error()));
    }

    // 验证数据库数量
    if (snapshot.databases.size() > MaxEntryCount) [[unlikely]] {
        return std::unexpected(make_error(
            CatalogErrorCode::ValueTooLarge,
            "database count is too large",
            {
                .operation = CatalogOperation::Encode,
                .path = path_,
            }
        ));
    }
    auto database_count_size = static_cast<std::uint32_t>(snapshot.databases.size());
    auto database_count = payload_writer.write_u32(database_count_size);
    if (!database_count) [[unlikely]] {
        return std::unexpected(std::move(database_count.error()));
    }

    for (const auto & database : snapshot.databases) {
        // 写入数据库信息

        auto database_id = payload_writer.write_u64(database.id);
        if (!database_id) [[unlikely]] {
            return std::unexpected(std::move(database_id.error()));
        }

        if (database.name.size() > MaxStringSize) [[unlikely]] {
            return std::unexpected(make_error(
                CatalogErrorCode::ResourceLimitExceeded,
                "database name exceeds limit",
                {
                    .operation = CatalogOperation::Encode,
                    .path = path_,
                }
            ));
        }
        auto database_name = payload_writer.write_string(database.name);
        if (!database_name) [[unlikely]] {
            return std::unexpected(std::move(database_name.error()));
        }

        // 验证集合数量
        if (database.collections.size() > MaxEntryCount) [[unlikely]] {
            return std::unexpected(make_error(
                CatalogErrorCode::ValueTooLarge,
                "collection count is too large",
                {
                    .operation = CatalogOperation::Encode,
                    .path = path_,
                }
            ));
        }
        auto collection_count_size = static_cast<std::uint32_t>(database.collections.size());
        auto collection_count = payload_writer.write_u32(collection_count_size);
        if (!collection_count) [[unlikely]] {
            return std::unexpected(std::move(collection_count.error()));
        }

        for (const auto & collection : database.collections) {
            // 写入集合信息

            auto collection_id = payload_writer.write_u64(collection.id);
            if (!collection_id) [[unlikely]] {
                return std::unexpected(std::move(collection_id.error()));
            }

            auto collection_database_id = payload_writer.write_u64(collection.database_id);
            if (!collection_database_id) [[unlikely]] {
                return std::unexpected(std::move(collection_database_id.error()));
            }

            if (collection.name.size() > MaxStringSize) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ResourceLimitExceeded,
                    "collection name exceeds limit",
                    {
                        .operation = CatalogOperation::Encode,
                        .path = path_,
                    }
                ));
            }
            auto collection_name = payload_writer.write_string(collection.name);
            if (!collection_name) [[unlikely]] {
                return std::unexpected(std::move(collection_name.error()));
            }

            auto comment_present_value = collection.comment.has_value();
            auto comment_present = payload_writer.write_u8(comment_present_value ? 1U : 0U);
            if (!comment_present) [[unlikely]] {
                return std::unexpected(std::move(comment_present.error()));
            }
            if (comment_present_value) {
                if (collection.comment.value().size() > MaxStringSize) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "collection comment exceeds limit",
                        {
                            .operation = CatalogOperation::Encode,
                            .path = path_,
                        }
                    ));
                }
                auto comment = payload_writer.write_string(collection.comment.value());
                if (!comment) [[unlikely]] {
                    return std::unexpected(std::move(comment.error()));
                }
            }

            // 验证列数量
            if (collection.columns.size() > MaxEntryCount) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ValueTooLarge,
                    "column count is too large",
                    {
                        .operation = CatalogOperation::Encode,
                        .path = path_,
                    }
                ));
            }
            auto column_count_size = static_cast<std::uint32_t>(collection.columns.size());
            auto column_count = payload_writer.write_u32(column_count_size);
            if (!column_count) [[unlikely]] {
                return std::unexpected(std::move(column_count.error()));
            }

            for (const auto & column : collection.columns) {
                // 写入列信息

                auto column_id = payload_writer.write_u64(column.id);
                if (!column_id) [[unlikely]] {
                    return std::unexpected(std::move(column_id.error()));
                }

                if (column.name.size() > MaxStringSize) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "column name exceeds limit",
                        {
                            .operation = CatalogOperation::Encode,
                            .path = path_,
                        }
                    ));
                }
                auto column_name = payload_writer.write_string(column.name);
                if (!column_name) [[unlikely]] {
                    return std::unexpected(std::move(column_name.error()));
                }

                auto column_type_id =
                    payload_writer.write_u8(static_cast<std::uint8_t>(column.type.id));
                if (!column_type_id) [[unlikely]] {
                    return std::unexpected(std::move(column_type_id.error()));
                }

                auto column_type_parameter_present_value = column.type.parameter.has_value();
                auto column_type_parameter_present =
                    payload_writer.write_u8(column_type_parameter_present_value ? 1U : 0U);
                if (!column_type_parameter_present) [[unlikely]] {
                    return std::unexpected(std::move(column_type_parameter_present.error()));
                }
                if (column_type_parameter_present_value) {
                    auto column_type_parameter = payload_writer.write_u64(
                        static_cast<std::uint64_t>(*column.type.parameter)
                    );
                    if (!column_type_parameter) [[unlikely]] {
                        return std::unexpected(std::move(column_type_parameter.error()));
                    }
                }

                auto column_unique = payload_writer.write_u8(column.unique ? 1U : 0U);
                if (!column_unique) [[unlikely]] {
                    return std::unexpected(std::move(column_unique.error()));
                }

                auto column_nullable = payload_writer.write_u8(column.nullable ? 1U : 0U);
                if (!column_nullable) [[unlikely]] {
                    return std::unexpected(std::move(column_nullable.error()));
                }

                auto column_default_expression_present_value =
                    column.default_expression.has_value();
                auto column_default_expression_present =
                    payload_writer.write_u8(column_default_expression_present_value ? 1U : 0U);
                if (!column_default_expression_present) [[unlikely]] {
                    return std::unexpected(std::move(column_default_expression_present.error()));
                }
                if (column_default_expression_present_value) {
                    auto column_default_expression =
                        write_default_expression(payload_writer, *column.default_expression, path_);
                    if (!column_default_expression) [[unlikely]] {
                        return std::unexpected(std::move(column_default_expression.error()));
                    }
                }

                auto column_comment_present_value = column.comment.has_value();
                auto column_comment_present =
                    payload_writer.write_u8(column_comment_present_value ? 1U : 0U);
                if (!column_comment_present) [[unlikely]] {
                    return std::unexpected(std::move(column_comment_present.error()));
                }
                if (column_comment_present_value) {
                    if (column.comment.value().size() > MaxStringSize) [[unlikely]] {
                        return std::unexpected(make_error(
                            CatalogErrorCode::ResourceLimitExceeded,
                            "column comment exceeds limit",
                            {
                                .operation = CatalogOperation::Encode,
                                .path = path_,
                            }
                        ));
                    }
                    auto column_comment = payload_writer.write_string(column.comment.value());
                    if (!column_comment) [[unlikely]] {
                        return std::unexpected(std::move(column_comment.error()));
                    }
                }
            }

            // 验证索引数量
            if (collection.indexes.size() > MaxEntryCount) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ValueTooLarge,
                    "index count is too large",
                    {
                        .operation = CatalogOperation::Encode,
                        .path = path_,
                    }
                ));
            }
            auto index_count_size = static_cast<std::uint32_t>(collection.indexes.size());
            auto index_count = payload_writer.write_u32(index_count_size);
            if (!index_count) [[unlikely]] {
                return std::unexpected(std::move(index_count.error()));
            }

            for (const auto & index : collection.indexes) {
                // 写入索引信息

                auto index_id = payload_writer.write_u64(index.id);
                if (!index_id) [[unlikely]] {
                    return std::unexpected(std::move(index_id.error()));
                }

                auto index_column_id = payload_writer.write_u64(index.column_id);
                if (!index_column_id) [[unlikely]] {
                    return std::unexpected(std::move(index_column_id.error()));
                }

                if (index.name.size() > MaxStringSize) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "index name exceeds limit",
                        {
                            .operation = CatalogOperation::Encode,
                            .path = path_,
                        }
                    ));
                }
                auto index_name = payload_writer.write_string(index.name);
                if (!index_name) [[unlikely]] {
                    return std::unexpected(std::move(index_name.error()));
                }

                auto index_kind =
                    payload_writer.write_u8(static_cast<std::uint8_t>(index.index_kind));
                if (!index_kind) [[unlikely]] {
                    return std::unexpected(std::move(index_kind.error()));
                }

                auto index_unique = payload_writer.write_u8(index.unique ? 1U : 0U);
                if (!index_unique) [[unlikely]] {
                    return std::unexpected(std::move(index_unique.error()));
                }
            }

            // 验证向量索引数量
            if (collection.vector_indexes.size() > MaxEntryCount) [[unlikely]] {
                return std::unexpected(make_error(
                    CatalogErrorCode::ValueTooLarge,
                    "vector index count is too large",
                    {
                        .operation = CatalogOperation::Encode,
                        .path = path_,
                    }
                ));
            }
            auto vector_index_count_size =
                static_cast<std::uint32_t>(collection.vector_indexes.size());
            auto vector_index_count = payload_writer.write_u32(vector_index_count_size);
            if (!vector_index_count) [[unlikely]] {
                return std::unexpected(std::move(vector_index_count.error()));
            }

            for (const auto & vector_index : collection.vector_indexes) {
                // 写入向量索引信息

                auto vector_index_id = payload_writer.write_u64(vector_index.id);
                if (!vector_index_id) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_id.error()));
                }

                auto vector_index_column_id = payload_writer.write_u64(vector_index.column_id);
                if (!vector_index_column_id) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_column_id.error()));
                }

                if (vector_index.name.size() > MaxStringSize) [[unlikely]] {
                    return std::unexpected(make_error(
                        CatalogErrorCode::ResourceLimitExceeded,
                        "vector index name exceeds limit",
                        {
                            .operation = CatalogOperation::Encode,
                            .path = path_,
                        }
                    ));
                }
                auto vector_index_name = payload_writer.write_string(vector_index.name);
                if (!vector_index_name) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_name.error()));
                }

                auto vector_index_kind =
                    payload_writer.write_u8(static_cast<std::uint8_t>(vector_index.index_kind));
                if (!vector_index_kind) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_kind.error()));
                }

                auto vector_index_metric =
                    payload_writer.write_u8(static_cast<std::uint8_t>(vector_index.metric));
                if (!vector_index_metric) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_metric.error()));
                }

                auto vector_index_dimension =
                    payload_writer.write_u64(static_cast<std::uint64_t>(vector_index.dimension));
                if (!vector_index_dimension) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_dimension.error()));
                }

                auto vector_index_max_neighbors = payload_writer.write_u64(
                    static_cast<std::uint64_t>(vector_index.max_neighbors)
                );
                if (!vector_index_max_neighbors) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_max_neighbors.error()));
                }

                auto vector_index_ef_construction = payload_writer.write_u64(
                    static_cast<std::uint64_t>(vector_index.ef_construction)
                );
                if (!vector_index_ef_construction) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_ef_construction.error()));
                }

                auto vector_index_ef_search_default = payload_writer.write_u64(
                    static_cast<std::uint64_t>(vector_index.ef_search_default)
                );
                if (!vector_index_ef_search_default) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_ef_search_default.error()));
                }

                auto vector_index_random_seed =
                    payload_writer.write_u64(static_cast<std::uint64_t>(vector_index.random_seed));
                if (!vector_index_random_seed) [[unlikely]] {
                    return std::unexpected(std::move(vector_index_random_seed.error()));
                }
            }
        }
    }

    // payload_bytes 大小已经在 BufferByteWriter 中确保小于分配大小
    // 如果超出，在上方写入时已经错误，这里不需要再检查

    // 计算校验和
    auto checksum = io::crc32(payload_bytes.bytes());

    io::BufferByteWriter encoded_bytes {
        static_cast<std::size_t>(CatalogHeaderSize + MaxPayloadSize)
    };
    io::LittleEndianBinaryWriter encoded_writer {encoded_bytes};

    // 写入头信息

    auto header_magic = encoded_writer.write_u32(CatalogMagic);
    if (!header_magic) [[unlikely]] {
        return std::unexpected(std::move(header_magic.error()));
    }

    auto header_version = encoded_writer.write_u16(CatalogVersion);
    if (!header_version) [[unlikely]] {
        return std::unexpected(std::move(header_version.error()));
    }

    auto header_header_size = encoded_writer.write_u16(CatalogHeaderSize);
    if (!header_header_size) [[unlikely]] {
        return std::unexpected(std::move(header_header_size.error()));
    }

    auto header_payload_size = encoded_writer.write_u64(payload_bytes.bytes().size());
    if (!header_payload_size) [[unlikely]] {
        return std::unexpected(std::move(header_payload_size.error()));
    }

    auto header_checksum = encoded_writer.write_u32(checksum);
    if (!header_checksum) [[unlikely]] {
        return std::unexpected(std::move(header_checksum.error()));
    }

    auto header_flag = encoded_writer.write_u32(0);
    if (!header_flag) [[unlikely]] {
        return std::unexpected(std::move(header_flag.error()));
    }

    // 写入负载数据
    auto encoded = encoded_bytes.write_bytes(payload_bytes.bytes());
    if (!encoded) [[unlikely]] {
        return std::unexpected(std::move(encoded.error()));
    }

    // 发布新的元数据文件

    // 确保元数据目录已存在
    const auto parent = path_.parent_path();
    if (!parent.empty()) {
        auto created = filesystem_->create_dir_all(parent);
        if (!created) {
            return std::unexpected(std::move(created.error()));
        }
    }

    // 生成临时文件路径
    auto tmp_path = path_;
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    tmp_path += std::format(
        ".{}.tmp.{}",
        timestamp,
        next_sequence_id.fetch_add(1, std::memory_order_relaxed)
    );

    // 创建临时文件
    auto file = filesystem_->open(
        tmp_path,
        {.access = filesystem::FileAccess::ReadWrite,
         .create_mode = filesystem::FileCreateMode::CreateNew}
    );
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }

    // 创建临时文件清理器
    TempFileCleanup cleanup {*filesystem_, tmp_path};

    // 写入临时文件
    io::FileByteWriter byte_writer {*file};
    auto written = byte_writer.write_bytes(encoded_bytes.bytes());
    if (!written) [[unlikely]] {
        return std::unexpected(std::move(written.error()));
    }

    // 同步临时文件
    auto synced = file->sync_all();
    if (!synced) [[unlikely]] {
        return std::unexpected(std::move(synced.error()));
    }

    // 关闭临时文件
    auto closed = file->close();
    if (!closed) [[unlikely]] {
        return std::unexpected(std::move(closed.error()));
    }

    // 原子替换旧的元数据文件
    auto replaced = filesystem_->replace_file_atomic(tmp_path, path_);
    if (!replaced) [[unlikely]] {
        return std::unexpected(std::move(replaced.error()));
    }

    // 原子替换成功后，临时文件已经消失，取消自动删除临时文件
    cleanup.release();

    // 同步元数据文件的父目录
    if (!parent.empty()) {
        auto directory_synced = filesystem_->sync_directory(parent);
        if (!directory_synced &&
            !directory_synced.error().is(filesystem::FileSystemErrorCode::Unsupported))
            [[unlikely]] {
            return std::unexpected(std::move(directory_synced.error()));
        }
    }

    return {};
}

} // namespace litedb::core::catalog
