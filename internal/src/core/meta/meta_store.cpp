#include "core/meta/meta_store.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/checksum.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/io/io_helper.hpp"

namespace litedb::core::meta
{

namespace
{

constexpr std::uint32_t MetaMagic = 0x544d444c;         // LDMT
constexpr std::uint16_t MetaFormatVersion = 2;
constexpr std::uint16_t MetaHeaderSize = 24;
constexpr std::uint64_t MaxPayloadSize = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t MaxStringSize = 1024ULL * 1024ULL;
constexpr std::uint32_t MaxEntryCount = 1'000'000U;
constexpr std::size_t MaxExpressionDepth = 64;
std::atomic<std::uint64_t> TempSequence {1};

class TempFileCleanup
{
public:
    TempFileCleanup(filesystem::FileSystem & filesystem, std::filesystem::path path)
        : filesystem_(&filesystem), path_(std::move(path))
    {
    }

    ~TempFileCleanup()
    {
        if (active_) {
            (void) filesystem_->remove(path_);
        }
    }

    void release() noexcept { active_ = false; }

private:
    filesystem::FileSystem * filesystem_;
    std::filesystem::path path_;
    bool active_ {true};
};

/**
 * @brief 从文件系统错误创建元数据存储错误
 * @param error 文件系统错误
 * @return 元数据存储错误
 */
[[nodiscard]]
MetaError from_filesystem_error(
    error::Error source,
    MetaOperation operation,
    const std::filesystem::path & path,
    MetaErrorCode code = MetaErrorCode::FileSystemFailure
)
{
    const auto source_code = source.encode_code();
    return make_error(code, source.message(), {
        .operation = operation,
        .path = path,
        .source_code = source_code,
    });
}

/**
 * @brief 从 IO 错误创建元数据存储错误
 * @param error  IO 错误
 * @return 元数据存储错误
 */
[[nodiscard]]
MetaError from_io_error(
    io::IoError source,
    MetaOperation operation,
    const std::filesystem::path & path = {}
)
{
    auto code = source.category() == error::ErrorCategory::FileSystem
        ? MetaErrorCode::FileSystemFailure
        : MetaErrorCode::IoFailure;
    if (source.is(io::IoErrorCode::UnexpectedEof)) code = MetaErrorCode::UnexpectedEof;
    else if (source.is(io::IoErrorCode::InvalidData)) code = MetaErrorCode::InvalidFormat;
    else if (source.is(io::IoErrorCode::ValueTooLarge)) code = MetaErrorCode::ValueTooLarge;
    const auto source_code = source.encode_code();
    return make_error(code, source.message(), {
        .operation = operation,
        .path = path,
        .source_code = source_code,
    });
}

/**
 * @brief 创建 IO 错误
 * @param message 错误消息
 * @return IO 错误
 */
[[nodiscard]]
io::IoError invalid_data(std::string message)
{
    return io::make_io_error(io::IoErrorCode::InvalidData, message);
}

/**
 * @brief 检查数量是否超出范围
 * @param count 数量
 * @return 检查后的数量
 */
[[nodiscard]]
std::expected<std::uint32_t, io::IoError> checked_count(std::size_t count)
{
    if (count > MaxEntryCount) {
        return std::unexpected(io::make_io_error(io::IoErrorCode::ValueTooLarge, "meta collection is too large"));
    }
    return static_cast<std::uint32_t>(count);
}

/**
 * @brief 编码器写入器
 * @param writer 二进制写入器
 */
class CodecWriter
{
public:
    explicit CodecWriter(io::BinaryWriter & writer) noexcept
        : writer_(&writer)
    {
    }

public:
    /**
     * @brief 写入 8 位无符号整数
     * @param value 值
     */
    void write_u8(std::uint8_t value)
    {
        write(writer_->write_u8(value));
    }

    /**
     * @brief 写入 16 位无符号整数
     * @param value 值
     */
    void write_u16(std::uint16_t value)
    {
        write(writer_->write_u16(value));
    }

    /**
     * @brief 写入 32 位无符号整数
     * @param value 值
     */
    void write_u32(std::uint32_t value)
    {
        write(writer_->write_u32(value));
    }

    /**
     * @brief 写入 64 位无符号整数
     * @param value 值
     */
    void write_u64(std::uint64_t value)
    {
        write(writer_->write_u64(value));
    }

    /**
     * @brief 写入字符串
     * @param value 值
     */
    void write_string(const std::string & value)
    {
        if (value.size() > MaxStringSize) {
            if (!error_) {
                error_ = io::make_io_error(io::IoErrorCode::ValueTooLarge, "meta string exceeds limit");
            }
            return;
        }
        write(writer_->write_string(value));
    }

    /**
     * @brief 写入数量
     * @param count 数量
     */
    void write_count(std::size_t count)
    {
        if (error_) {
            return;
        }
        auto value = checked_count(count);
        if (!value) {
            error_ = std::move(value.error());
            return;
        }
        write_u32(*value);
    }

    /**
     * @brief 获取结果
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, io::IoError> take_result()
    {
        if (error_) {
            return std::unexpected(std::move(*error_));
        }
        return {};
    }

private:
    /**
     * @brief 判断是否写入成功
     * @param result 写入结果
     */
    void write(std::expected<void, io::IoError> result)
    {
        if (!error_ && !result) {
            error_ = std::move(result.error());
        }
    }

private:
    io::BinaryWriter * writer_;           ///< 二进制写入器
    std::optional<io::IoError> error_;    ///< 错误
};

/**
 * @brief 编码器读取器
 * @param reader 二进制读取器
 */
class CodecReader
{
public:
    explicit CodecReader(io::BinaryReader & reader) noexcept
        : reader_(&reader)
    {
    }

public:
    /**
     * @brief 读取 8 位无符号整数
     * @return 值
     */
    std::uint8_t read_u8()
    {
        return read(reader_->read_u8());
    }

    /**
     * @brief 读取 16 位无符号整数
     * @return 值
     */
    std::uint16_t read_u16()
    {
        return read(reader_->read_u16());
    }

    /**
     * @brief 读取 32 位无符号整数
     * @return 值
     */
    std::uint32_t read_u32()
    {
        return read(reader_->read_u32());
    }

    /**
     * @brief 读取 64 位无符号整数
     * @return 值
     */
    std::uint64_t read_u64()
    {
        return read(reader_->read_u64());
    }

    std::size_t read_size()
    {
        const auto value = read_u64();
        if (value > std::numeric_limits<std::size_t>::max()) {
            fail("encoded size does not fit this platform");
            return 0;
        }
        return static_cast<std::size_t>(value);
    }

    /**
     * @brief 读取字符串
     * @return 值
     */
    std::string read_string()
    {
        return read(reader_->read_string());
    }

    std::uint32_t read_count()
    {
        const auto value = read_u32();
        if (value > MaxEntryCount) {
            fail(io::IoErrorCode::ValueTooLarge, "meta entry count exceeds limit");
            return 0;
        }
        return value;
    }

    /**
     * @brief 设置错误
     * @param message 错误消息
     */
    void fail(std::string message)
    {
        if (!error_) {
            error_ = invalid_data(std::move(message));
        }
    }

    void fail(io::IoErrorCode code, std::string message)
    {
        if (!error_) {
            error_ = io::make_io_error(code, std::move(message));
        }
    }

    /**
     * @brief 判断是否读取成功
     * @return 是否成功
     */
    [[nodiscard]]
    bool ok() const noexcept { return !error_.has_value(); }

    /**
     * @brief 获取结果
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, io::IoError> take_result()
    {
        if (error_) {
            return std::unexpected(std::move(*error_));
        }
        return {};
    }

private:
    /**
     * @brief 读取值
     * @tparam T 值类型
     * @param result 读取结果
     * @return 值
     */
    template <typename T>
    T read(std::expected<T, io::IoError> result)
    {
        if (error_) {
            return T {};
        }
        if (!result) {
            error_ = std::move(result.error());
            return T {};
        }
        return std::move(*result);
    }

private:
    io::BinaryReader * reader_;           ///< 二进制读取器
    std::optional<io::IoError> error_;    ///< 错误
};

/**
 * @brief 写入可选大小
 * @param writer 编码器写入器
 * @param value 值
 */
void write_optional_size(CodecWriter & writer, const std::optional<std::size_t> & value)
{
    writer.write_u8(value ? 1U : 0U);
    if (value) {
        writer.write_u64(static_cast<std::uint64_t>(*value));
    }
}

/**
 * @brief 读取可选大小
 * @param reader 编码器读取器
 * @return 值
 */
std::optional<std::size_t> read_optional_size(CodecReader & reader)
{
    const auto present = reader.read_u8();
    if (!reader.ok()) {
        return std::nullopt;
    }
    if (present > 1) {
        reader.fail("invalid optional size marker");
        return std::nullopt;
    }
    return present == 0 ? std::nullopt
                        : std::optional<std::size_t> {reader.read_size()};
}

/**
 * @brief 写入可选字符串
 * @param writer 编码器写入器
 * @param value 值
 */
void write_optional_string(CodecWriter & writer, const std::optional<std::string> & value)
{
    writer.write_u8(value ? 1U : 0U);
    if (value) {
        writer.write_string(*value);
    }
}

/**
 * @brief 读取可选字符串
 * @param reader 编码器读取器
 * @return 值
 */
std::optional<std::string> read_optional_string(CodecReader & reader)
{
    const auto present = reader.read_u8();
    if (!reader.ok()) {
        return std::nullopt;
    }
    if (present > 1) {
        reader.fail("invalid optional string marker");
        return std::nullopt;
    }
    return present == 0 ? std::nullopt : std::optional<std::string> {reader.read_string()};
}

/**
 * @brief 写入默认表达式
 * @param writer 编码器写入器
 * @param expression 表达式
 */
void write_default_expression(CodecWriter & writer, const schema::DefaultExpression & expression)
{
    writer.write_u8(static_cast<std::uint8_t>(expression.kind));
    writer.write_u8(static_cast<std::uint8_t>(expression.literal_kind));
    writer.write_string(expression.value);
    writer.write_count(expression.elements.size());
    for (const auto & element : expression.elements) {
        write_default_expression(writer, element);
    }
}

/**
 * @brief 读取默认表达式
 * @param reader 编码器读取器
 * @return 表达式
 */
schema::DefaultExpression read_default_expression(CodecReader & reader, std::size_t depth = 0)
{
    schema::DefaultExpression expression;
    if (depth >= MaxExpressionDepth) {
        reader.fail("default expression nesting exceeds limit");
        return expression;
    }
    const auto expression_kind = reader.read_u8();
    const auto literal_kind = reader.read_u8();
    if (expression_kind > static_cast<std::uint8_t>(schema::DefaultExpressionKind::Vector)) {
        reader.fail("invalid default expression kind");
    }
    if (literal_kind > static_cast<std::uint8_t>(schema::DefaultLiteralKind::String)) {
        reader.fail("invalid default literal kind");
    }
    expression.kind = static_cast<schema::DefaultExpressionKind>(expression_kind);
    expression.literal_kind = static_cast<schema::DefaultLiteralKind>(literal_kind);
    expression.value = reader.read_string();
    const auto count = reader.read_count();
    if (!reader.ok()) {
        return expression;
    }
    expression.elements.reserve(count);
    for (std::uint32_t i = 0; i < count && reader.ok(); ++i) {
        expression.elements.push_back(read_default_expression(reader, depth + 1));
    }
    return expression;
}

/**
 * @brief 写入可选表达式
 * @param writer 编码器写入器
 * @param value 值
 */
void write_optional_expression(CodecWriter & writer, const std::optional<schema::DefaultExpression> & value)
{
    writer.write_u8(value ? 1U : 0U);
    if (value) {
        write_default_expression(writer, *value);
    }
}

/**
 * @brief 读取可选表达式
 * @param reader 编码器读取器
 * @return 表达式
 */
std::optional<schema::DefaultExpression> read_optional_expression(CodecReader & reader)
{
    const auto present = reader.read_u8();
    if (!reader.ok()) {
        return std::nullopt;
    }
    if (present > 1) {
        reader.fail("invalid optional expression marker");
        return std::nullopt;
    }
    return present == 0 ? std::nullopt : std::optional<schema::DefaultExpression> {read_default_expression(reader)};
}

/**
 * @brief 读取布尔值
 * @param reader 编码器读取器
 * @return 值
 */
bool read_bool(CodecReader & reader)
{
    const auto value = reader.read_u8();
    if (value > 1) {
        reader.fail("invalid boolean value");
    }
    return value != 0;
}

/**
 * @brief 写入元数据快照
 * @param writer 二进制写入器
 * @param snapshot 元数据快照
 * @return 结果
 */
std::expected<void, io::IoError> write_snapshot(io::BinaryWriter & binary_writer, const MetaSnapshot & snapshot)
{
    CodecWriter writer {binary_writer};
    writer.write_u64(snapshot.next_database_id);
    writer.write_u64(snapshot.next_collection_id);
    writer.write_u64(snapshot.next_column_id);
    writer.write_u64(snapshot.next_index_id);
    writer.write_u64(snapshot.next_vector_index_id);
    writer.write_count(snapshot.databases.size());
    for (const auto & database : snapshot.databases) {
        writer.write_u64(database.id);
        writer.write_string(database.name);
        writer.write_count(database.collections.size());
        for (const auto & collection : database.collections) {
            writer.write_u64(collection.id);
            writer.write_u64(collection.database_id);
            writer.write_string(collection.name);
            write_optional_string(writer, collection.comment);
            writer.write_count(collection.columns.size());
            for (const auto & column : collection.columns) {
                writer.write_u64(column.id);
                writer.write_string(column.name);
                writer.write_u8(static_cast<std::uint8_t>(column.type.id));
                write_optional_size(writer, column.type.parameter);
                writer.write_u8(column.unique ? 1U : 0U);
                writer.write_u8(column.nullable ? 1U : 0U);
                write_optional_expression(writer, column.default_expression);
                write_optional_string(writer, column.comment);
            }
            writer.write_count(collection.indexes.size());
            for (const auto & index : collection.indexes) {
                writer.write_u64(index.id);
                writer.write_count(index.column_ids.size());
                for (const auto column_id : index.column_ids) {
                    writer.write_u64(column_id);
                }
                writer.write_string(index.name);
                writer.write_u8(static_cast<std::uint8_t>(index.index_kind));
                writer.write_u8(index.unique ? 1U : 0U);
            }
            writer.write_count(collection.vector_indexes.size());
            for (const auto & index : collection.vector_indexes) {
                writer.write_u64(index.id);
                writer.write_u64(index.column_id);
                writer.write_string(index.name);
                writer.write_u8(static_cast<std::uint8_t>(index.index_kind));
                writer.write_u8(static_cast<std::uint8_t>(index.metric));
                writer.write_u64(index.dimension);
                writer.write_u64(index.max_neighbors);
                writer.write_u64(index.ef_construction);
                writer.write_u64(index.ef_search_default);
                writer.write_u64(index.random_seed);
            }
        }
    }
    return writer.take_result();
}

/**
 * @brief 读取元数据快照
 * @param reader 二进制读取器
 * @return 元数据快照
 */
std::expected<MetaSnapshot, MetaError> read_snapshot(
    io::BinaryReader & binary_reader,
    const std::filesystem::path & path
)
{
    CodecReader reader {binary_reader};
    MetaSnapshot snapshot;
    snapshot.next_database_id = reader.read_u64();
    snapshot.next_collection_id = reader.read_u64();
    snapshot.next_column_id = reader.read_u64();
    snapshot.next_index_id = reader.read_u64();
    snapshot.next_vector_index_id = reader.read_u64();
    const auto database_count = reader.read_count();
    if (!reader.ok()) {
        auto failed = reader.take_result();
        return std::unexpected(from_io_error(std::move(failed.error()), MetaOperation::Decode, path));
    }
    snapshot.databases.reserve(database_count);
    for (std::uint32_t d = 0; d < database_count && reader.ok(); ++d) {
        MetaSnapshotDatabase database;
        database.id = reader.read_u64();
        database.name = reader.read_string();
        const auto collection_count = reader.read_count();
        database.collections.reserve(collection_count);
        for (std::uint32_t c = 0; c < collection_count && reader.ok(); ++c) {
            MetaSnapshotCollection collection;
            collection.id = reader.read_u64();
            collection.database_id = reader.read_u64();
            collection.name = reader.read_string();
            collection.comment = read_optional_string(reader);
            const auto column_count = reader.read_count();
            collection.columns.reserve(column_count);
            for (std::uint32_t n = 0; n < column_count && reader.ok(); ++n) {
                MetaSnapshotColumn column;
                column.id = reader.read_u64();
                column.name = reader.read_string();
                const auto type_id = reader.read_u8();
                if (type_id > static_cast<std::uint8_t>(common::LogicalTypeId::Vector)) {
                    reader.fail("invalid logical type id");
                }
                column.type.id = static_cast<common::LogicalTypeId>(type_id);
                column.type.parameter = read_optional_size(reader);
                column.unique = read_bool(reader);
                column.nullable = read_bool(reader);
                column.default_expression = read_optional_expression(reader);
                column.comment = read_optional_string(reader);
                collection.columns.push_back(std::move(column));
            }
            const auto index_count = reader.read_count();
            collection.indexes.reserve(index_count);
            for (std::uint32_t n = 0; n < index_count && reader.ok(); ++n) {
                MetaSnapshotIndex index;
                index.id = reader.read_u64();
                const auto index_column_count = reader.read_count();
                index.column_ids.reserve(index_column_count);
                for (std::uint32_t k = 0; k < index_column_count && reader.ok(); ++k) {
                    index.column_ids.push_back(reader.read_u64());
                }
                index.name = reader.read_string();
                const auto index_kind = reader.read_u8();
                if (index_kind != static_cast<std::uint8_t>(entry::IndexKind::BTree)) {
                    reader.fail("invalid index kind");
                }
                index.index_kind = static_cast<entry::IndexKind>(index_kind);
                index.unique = read_bool(reader);
                collection.indexes.push_back(std::move(index));
            }
            const auto vector_index_count = reader.read_count();
            collection.vector_indexes.reserve(vector_index_count);
            for (std::uint32_t n = 0; n < vector_index_count && reader.ok(); ++n) {
                MetaSnapshotVectorIndex index;
                index.id = reader.read_u64();
                index.column_id = reader.read_u64();
                index.name = reader.read_string();
                const auto index_kind = reader.read_u8();
                const auto metric = reader.read_u8();
                if (index_kind > static_cast<std::uint8_t>(entry::VectorIndexKind::Hnsw)) {
                    reader.fail("invalid vector index kind");
                }
                if (metric > static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine)) {
                    reader.fail("invalid vector distance metric");
                }
                index.index_kind = static_cast<entry::VectorIndexKind>(index_kind);
                index.metric = static_cast<entry::VectorDistanceMetric>(metric);
                index.dimension = reader.read_size();
                index.max_neighbors = reader.read_size();
                index.ef_construction = reader.read_size();
                index.ef_search_default = reader.read_size();
                index.random_seed = reader.read_size();
                collection.vector_indexes.push_back(std::move(index));
            }
            database.collections.push_back(std::move(collection));
        }
        snapshot.databases.push_back(std::move(database));
    }
    if (auto result = reader.take_result(); !result) {
        return std::unexpected(from_io_error(std::move(result.error()), MetaOperation::Decode, path));
    }
    return snapshot;
}

} // namespace

MetaStore::MetaStore(std::filesystem::path path, filesystem::FileSystem & filesystem)
    : path_(std::move(path))
    , filesystem_(&filesystem)
{
}

std::expected<std::optional<MetaSnapshot>, MetaError> MetaStore::load() const
{
    auto exists = filesystem_->exists(path_);
    if (!exists) {
        return std::unexpected(from_filesystem_error(std::move(exists.error()), MetaOperation::Load, path_));
    }
    if (!*exists) {
        return std::optional<MetaSnapshot> {};
    }

    auto file = filesystem_->open(path_, {.access = filesystem::FileAccess::ReadOnly,
                                          .create_mode = filesystem::FileCreateMode::OpenExisting});
    if (!file) {
        return std::unexpected(from_filesystem_error(std::move(file.error()), MetaOperation::Load, path_));
    }
    auto file_size = file->size();
    if (!file_size) {
        return std::unexpected(from_filesystem_error(std::move(file_size.error()), MetaOperation::Load, path_));
    }
    if (*file_size < MetaHeaderSize) {
        return std::unexpected(make_error(MetaErrorCode::UnexpectedEof, "meta file header is truncated", {
            .operation = MetaOperation::Decode,
            .path = path_,
        }));
    }
    if (*file_size > MaxPayloadSize + MetaHeaderSize) {
        return std::unexpected(make_error(MetaErrorCode::ResourceLimitExceeded, "meta file exceeds size limit", {
            .operation = MetaOperation::Decode,
            .path = path_,
        }));
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(*file_size));
    io::FileByteReader file_reader {*file};
    if (auto read = file_reader.read_exact(bytes); !read) {
        return std::unexpected(from_io_error(std::move(read.error()), MetaOperation::Load, path_));
    }

    io::BufferByteReader header_source {std::span<const std::byte> {bytes}.first(MetaHeaderSize)};
    io::BinaryReader header {
        header_source,
        {.max_total_bytes = MetaHeaderSize, .max_string_bytes = 0},
    };
    auto magic = header.read_u32();
    auto version = header.read_u16();
    auto header_size = header.read_u16();
    auto payload_size = header.read_u64();
    auto payload_checksum = header.read_u32();
    auto flags = header.read_u32();
    if (!magic || !version || !header_size || !payload_size || !payload_checksum || !flags) {
        auto * failure = !magic ? &magic.error()
                        : !version ? &version.error()
                        : !header_size ? &header_size.error()
                        : !payload_size ? &payload_size.error()
                        : !payload_checksum ? &payload_checksum.error()
                        : &flags.error();
        return std::unexpected(from_io_error(std::move(*failure), MetaOperation::Decode, path_));
    }
    if (*magic != MetaMagic) {
        return std::unexpected(make_error(MetaErrorCode::InvalidFormat, "invalid meta file magic", {
            .operation = MetaOperation::Decode, .path = path_,
        }));
    }
    if (*version != MetaFormatVersion) {
        return std::unexpected(make_error(MetaErrorCode::UnsupportedVersion, "unsupported meta format version", {
            .operation = MetaOperation::Decode, .path = path_,
        }));
    }
    if (*header_size != MetaHeaderSize || *flags != 0) {
        return std::unexpected(make_error(MetaErrorCode::InvalidFormat, "invalid meta V2 header", {
            .operation = MetaOperation::Decode, .path = path_,
        }));
    }
    if (*payload_size > MaxPayloadSize || *payload_size != *file_size - MetaHeaderSize) {
        return std::unexpected(make_error(
            *payload_size > MaxPayloadSize ? MetaErrorCode::ResourceLimitExceeded : MetaErrorCode::InvalidFormat,
            "invalid meta payload size",
            {.operation = MetaOperation::Decode, .path = path_}
        ));
    }
    const auto payload = std::span<const std::byte> {bytes}.subspan(MetaHeaderSize);
    if (io::crc32(payload) != *payload_checksum) {
        return std::unexpected(make_error(MetaErrorCode::ChecksumMismatch, "meta payload checksum mismatch", {
            .operation = MetaOperation::Decode, .path = path_,
        }));
    }
    io::BufferByteReader payload_source {payload};
    io::BinaryReader reader {
        payload_source,
        {.max_total_bytes = *payload_size, .max_string_bytes = MaxStringSize},
    };
    auto snapshot = read_snapshot(reader, path_);
    if (!snapshot) {
        return std::unexpected(std::move(snapshot.error()));
    }
    return std::optional<MetaSnapshot> {std::move(*snapshot)};
}

std::expected<void, MetaError> MetaStore::save(const MetaSnapshot & snapshot) const
{
    io::BufferByteWriter payload_bytes {MaxPayloadSize};
    io::BinaryWriter payload_writer {payload_bytes};
    if (auto encoded = write_snapshot(payload_writer, snapshot); !encoded) {
        return std::unexpected(from_io_error(std::move(encoded.error()), MetaOperation::Encode, path_));
    }
    if (payload_bytes.bytes().size() > MaxPayloadSize) {
        return std::unexpected(make_error(MetaErrorCode::ResourceLimitExceeded, "meta payload exceeds size limit", {
            .operation = MetaOperation::Encode, .path = path_,
        }));
    }

    io::BufferByteWriter encoded_bytes {MaxPayloadSize + MetaHeaderSize};
    io::BinaryWriter encoded_writer {encoded_bytes};
    const auto checksum = io::crc32(payload_bytes.bytes());
    auto write_header = [&]() -> std::expected<void, io::IoError> {
        if (auto result = encoded_writer.write_u32(MetaMagic); !result) return result;
        if (auto result = encoded_writer.write_u16(MetaFormatVersion); !result) return result;
        if (auto result = encoded_writer.write_u16(MetaHeaderSize); !result) return result;
        if (auto result = encoded_writer.write_u64(payload_bytes.bytes().size()); !result) return result;
        if (auto result = encoded_writer.write_u32(checksum); !result) return result;
        if (auto result = encoded_writer.write_u32(0); !result) return result;
        return encoded_bytes.write_bytes(payload_bytes.bytes());
    };
    if (auto encoded = write_header(); !encoded) {
        return std::unexpected(from_io_error(std::move(encoded.error()), MetaOperation::Encode, path_));
    }

    const auto parent = path_.parent_path();
    if (!parent.empty()) {
        auto created = filesystem_->create_dir_all(parent);
        if (!created) {
            return std::unexpected(from_filesystem_error(
                std::move(created.error()), MetaOperation::SaveTemporary, path_
            ));
        }
    }
    auto temp_path = path_;
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    temp_path += ".tmp." + std::to_string(timestamp) + "."
               + std::to_string(TempSequence.fetch_add(1, std::memory_order_relaxed));
    TempFileCleanup cleanup {*filesystem_, temp_path};
    auto file = filesystem_->open(temp_path, {.access = filesystem::FileAccess::ReadWrite,
                                               .create_mode = filesystem::FileCreateMode::CreateNew});
    if (!file) {
        return std::unexpected(from_filesystem_error(
            std::move(file.error()), MetaOperation::SaveTemporary, temp_path
        ));
    }

    io::FileByteWriter byte_writer {*file};
    if (auto written = byte_writer.write_bytes(encoded_bytes.bytes()); !written) {
        return std::unexpected(from_io_error(std::move(written.error()), MetaOperation::SaveTemporary, temp_path));
    }
    if (auto synced = file->sync_all(); !synced) {
        return std::unexpected(from_filesystem_error(
            std::move(synced.error()), MetaOperation::SyncTemporary, temp_path
        ));
    }
    if (auto closed = file->close(); !closed) {
        return std::unexpected(from_filesystem_error(
            std::move(closed.error()), MetaOperation::SyncTemporary, temp_path
        ));
    }

    if (auto renamed = filesystem_->replace_file_atomic(temp_path, path_); !renamed) {
        return std::unexpected(from_filesystem_error(
            std::move(renamed.error()), MetaOperation::PublishFile, path_
        ));
    }
    cleanup.release();
    if (!parent.empty()) {
        auto synced = filesystem_->sync_directory(parent);
        if (!synced && !synced.error().is(filesystem::FileSystemErrorCode::Unsupported)) {
            return std::unexpected(from_filesystem_error(
                std::move(synced.error()),
                MetaOperation::SyncDirectory,
                parent,
                MetaErrorCode::DurabilityUnknown
            ));
        }
    }
    return {};
}

} // namespace litedb::core::meta
