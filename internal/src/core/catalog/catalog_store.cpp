#include "core/catalog/catalog_store.hpp"

#include "catalog_snapshot.hpp"
#include "core/catalog/catalog_constant.hpp"
#include "core/catalog/catalog_snapshot.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/checksum.hpp"
#include "core/schema/default_expression.hpp"

#include <expected>
#include <limits>

namespace litedb::core::catalog
{

namespace
{

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
    if (*expression_kind > static_cast<std::uint8_t>(schema::DefaultExpressionKind::Vector)) [[unlikely]] {
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
    if (*literal_kind > static_cast<std::uint8_t>(schema::DefaultLiteralKind::String)) [[unlikely]] {
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
    if (*element_count > 0 && *expression_kind == static_cast<std::uint8_t>(schema::DefaultExpressionKind::Literal)) {
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

} // namespace

CatalogStore::CatalogStore(std::filesystem::path path, filesystem::FileSystem & filesystem)
    : path_(std::move(path))
    , filesystem_(&filesystem)
{}

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
        {
            .access = filesystem::FileAccess::ReadOnly,
            .create_mode = filesystem::FileCreateMode::OpenExisting
        }
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
    std::vector<std::byte> header_bytes(static_cast<std::size_t>(CatalogHeaderSize));
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
    if (*version != CatalogFormatVersion) [[unlikely]] {
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
    if (*payload_size > MaxPayloadSize || *payload_size != *file_size - CatalogHeaderSize) [[unlikely]] {
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
        database.collections.reserve(*collection_count);

        // 读取每个集合快照
        for (std::uint32_t collection_index = 0; collection_index < *collection_count; ++collection_index) {
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
                if (*type_id > static_cast<std::uint8_t>(common::LogicalTypeId::Vector)) [[unlikely]] {
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
            collection.vector_indexes.reserve(*vector_index_count);

            // 读取每个向量索引快照
            for (std::uint32_t vector_index_index = 0; vector_index_index < *vector_index_count; ++vector_index_index) {
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
                if (*index_kind > static_cast<std::uint8_t>(entry::VectorIndexKind::Hnsw)) [[unlikely]] {
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
                if (*metric > static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine)) [[unlikely]] {
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

    return snapshot;
}

std::expected<void, CatalogError> CatalogStore::save(const CatalogSnapshot & snapshot) const
{

}

} // namespace litedb::core::catalog
