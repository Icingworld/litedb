#include "core/meta/meta_store.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/io/io_helper.hpp"
#include "core/meta/meta_helper.hpp"

namespace litedb::core::meta
{

namespace
{

constexpr std::uint32_t MetaMagic = 0x544d444c;         // LDMT
constexpr std::uint16_t MetaFormatVersion = 1;          // 元数据格式版本
constexpr std::uint16_t MetaHeaderSize = 8;             // 元数据头大小
constexpr std::size_t MaxMetaBytes = 64 * 1024 * 1024;
constexpr std::uint32_t MaxMetaStringBytes = 16 * 1024 * 1024;
constexpr std::uint32_t MaxMetaItems = 1'000'000;
constexpr std::uint32_t MaxUntrustedReserve = 1024;

/**
 * @brief 从文件系统错误创建元数据存储错误
 * @param error 文件系统错误
 * @return 元数据存储错误
 */
[[nodiscard]]
MetaStoreError from_filesystem_error(error::Error error)
{
    return make_error(MetaStoreErrorCode::FileSystemError, error.message());
}

/**
 * @brief 从 IO 错误创建元数据存储错误
 * @param error  IO 错误
 * @return 元数据存储错误
 */
[[nodiscard]]
MetaStoreError from_io_error(io::IoError error)
{
    if (error.category() == error::ErrorCategory::FileSystem) {
        return make_error(MetaStoreErrorCode::FileSystemError, error.message());
    }
    if (error.is(io::IoErrorCode::UnexpectedEof)) {
        return make_error(MetaStoreErrorCode::UnexpectedEof, error.message());
    }
    if (error.is(io::IoErrorCode::ValueTooLarge)) {
        return make_error(MetaStoreErrorCode::ValueTooLarge, error.message());
    }
    return make_error(MetaStoreErrorCode::InvalidFormat, error.message());
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
    if (count > std::numeric_limits<std::uint32_t>::max()) {
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
    CodecReader(io::BinaryReader & reader, std::uint32_t max_items) noexcept
        : reader_(&reader)
        , remaining_items_(max_items)
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

    /**
     * @brief 读取字符串
     * @return 值
     */
    std::string read_string()
    {
        return read(reader_->read_string());
    }

    std::uint32_t read_count(std::size_t minimum_encoded_bytes)
    {
        const auto count = read_u32();
        if (!ok()) {
            return 0;
        }
        if (count > remaining_items_) {
            fail(io::make_io_error(
                io::IoErrorCode::ValueTooLarge,
                "metadata item count exceeds the configured limit"
            ));
            return 0;
        }
        if (minimum_encoded_bytes != 0 &&
            count > reader_->remaining_bytes() / minimum_encoded_bytes) {
            fail(io::make_io_error(
                io::IoErrorCode::UnexpectedEof,
                "metadata item count exceeds the remaining binary data"
            ));
            return 0;
        }
        remaining_items_ -= count;
        return count;
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

    void fail(io::IoError error)
    {
        if (!error_) {
            error_ = std::move(error);
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
    std::uint32_t remaining_items_;       ///< 剩余集合元素预算
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
                        : std::optional<std::size_t> {static_cast<std::size_t>(reader.read_u64())};
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
schema::DefaultExpression read_default_expression(CodecReader & reader)
{
    schema::DefaultExpression expression;
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
    const auto count = reader.read_count(10);
    if (!reader.ok()) {
        return expression;
    }
    expression.elements.reserve(std::min(count, MaxUntrustedReserve));
    for (std::uint32_t i = 0; i < count && reader.ok(); ++i) {
        expression.elements.push_back(read_default_expression(reader));
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
    writer.write_u32(MetaMagic);
    writer.write_u16(MetaFormatVersion);
    writer.write_u16(MetaHeaderSize);
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
std::expected<MetaSnapshot, MetaStoreError> read_snapshot(io::BinaryReader & binary_reader)
{
    CodecReader reader {binary_reader, MaxMetaItems};
    if (reader.read_u32() != MetaMagic) {
        reader.fail("invalid meta file magic");
    }
    const auto version = reader.read_u16();
    if (reader.ok() && version != MetaFormatVersion) {
        return std::unexpected(make_error(MetaStoreErrorCode::UnsupportedVersion, "unsupported meta format version"));
    }
    if (reader.read_u16() < MetaHeaderSize) {
        reader.fail("invalid meta file header size");
    }

    MetaSnapshot snapshot;
    snapshot.next_database_id = reader.read_u64();
    snapshot.next_collection_id = reader.read_u64();
    snapshot.next_column_id = reader.read_u64();
    snapshot.next_index_id = reader.read_u64();
    snapshot.next_vector_index_id = reader.read_u64();
    const auto database_count = reader.read_count(16);
    if (!reader.ok()) {
        auto result = reader.take_result();
        return std::unexpected(from_io_error(std::move(result.error())));
    }
    snapshot.databases.reserve(std::min(database_count, MaxUntrustedReserve));
    for (std::uint32_t d = 0; d < database_count && reader.ok(); ++d) {
        MetaSnapshotDatabase database;
        database.id = reader.read_u64();
        database.name = reader.read_string();
        const auto collection_count = reader.read_count(33);
        database.collections.reserve(std::min(collection_count, MaxUntrustedReserve));
        for (std::uint32_t c = 0; c < collection_count && reader.ok(); ++c) {
            MetaSnapshotCollection collection;
            collection.id = reader.read_u64();
            collection.database_id = reader.read_u64();
            collection.name = reader.read_string();
            collection.comment = read_optional_string(reader);
            const auto column_count = reader.read_count(18);
            collection.columns.reserve(std::min(column_count, MaxUntrustedReserve));
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
            const auto index_count = reader.read_count(18);
            collection.indexes.reserve(std::min(index_count, MaxUntrustedReserve));
            for (std::uint32_t n = 0; n < index_count && reader.ok(); ++n) {
                MetaSnapshotIndex index;
                index.id = reader.read_u64();
                const auto index_column_count = reader.read_count(8);
                index.column_ids.reserve(std::min(index_column_count, MaxUntrustedReserve));
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
            const auto vector_index_count = reader.read_count(62);
            collection.vector_indexes.reserve(std::min(vector_index_count, MaxUntrustedReserve));
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
                index.dimension = static_cast<std::size_t>(reader.read_u64());
                index.max_neighbors = static_cast<std::size_t>(reader.read_u64());
                index.ef_construction = static_cast<std::size_t>(reader.read_u64());
                index.ef_search_default = static_cast<std::size_t>(reader.read_u64());
                index.random_seed = static_cast<std::size_t>(reader.read_u64());
                collection.vector_indexes.push_back(std::move(index));
            }
            database.collections.push_back(std::move(collection));
        }
        snapshot.databases.push_back(std::move(database));
    }
    if (auto result = reader.take_result(); !result) {
        return std::unexpected(from_io_error(std::move(result.error())));
    }
    return snapshot;
}

} // namespace

MetaStore::MetaStore(std::filesystem::path path, filesystem::FileSystem & filesystem)
    : path_(std::move(path))
    , filesystem_(&filesystem)
{
}

std::expected<MetaSnapshot, MetaStoreError> MetaStore::load() const
{
    auto exists = filesystem_->exists(path_);
    if (!exists) {
        return std::unexpected(from_filesystem_error(std::move(exists.error())));
    }
    if (!*exists) {
        return MetaSnapshot {};
    }

    auto file = filesystem_->open(path_, {.access = filesystem::FileAccess::ReadOnly,
                                         .create_mode = filesystem::FileCreateMode::OpenExisting});
    if (!file) {
        return std::unexpected(from_filesystem_error(std::move(file.error())));
    }
    auto file_size = file->size();
    if (!file_size) {
        return std::unexpected(from_filesystem_error(std::move(file_size.error())));
    }
    if (*file_size > MaxMetaBytes) {
        return std::unexpected(make_error(
            MetaStoreErrorCode::ValueTooLarge,
            "metadata file exceeds the configured size limit"
        ));
    }
    io::FileByteReader byte_reader {*file};
    io::BinaryReader reader {
        byte_reader,
        io::BinaryDecodeLimits {
            .max_total_bytes = *file_size,
            .max_string_bytes = MaxMetaStringBytes,
        },
    };
    return read_snapshot(reader);
}

std::expected<void, MetaStoreError> MetaStore::save(const MetaSnapshot & snapshot) const
{
    const auto parent = path_.parent_path();
    if (!parent.empty()) {
        auto created = filesystem_->create_dir_all(parent);
        if (!created) {
            return std::unexpected(from_filesystem_error(std::move(created.error())));
        }
    }
    auto temp_path = path_;
    temp_path += ".tmp";
    auto file = filesystem_->open(temp_path, {.access = filesystem::FileAccess::ReadWrite,
                                              .create_mode = filesystem::FileCreateMode::CreateOrTruncate});
    if (!file) {
        return std::unexpected(from_filesystem_error(std::move(file.error())));
    }

    io::BufferByteWriter encoded {MaxMetaBytes};
    io::BinaryWriter writer {encoded};
    if (auto written = write_snapshot(writer, snapshot); !written) {
        return std::unexpected(from_io_error(std::move(written.error())));
    }
    io::FileByteWriter byte_writer {*file};
    if (auto written = byte_writer.write_bytes(encoded.bytes()); !written) {
        return std::unexpected(from_io_error(std::move(written.error())));
    }
    if (auto synced = file->sync_all(); !synced) {
        return std::unexpected(from_filesystem_error(std::move(synced.error())));
    }
    if (auto closed = file->close(); !closed) {
        return std::unexpected(from_filesystem_error(std::move(closed.error())));
    }

    if (auto renamed = filesystem_->replace_file_atomic(temp_path, path_); !renamed) {
        return std::unexpected(from_filesystem_error(std::move(renamed.error())));
    }
    if (!parent.empty()) {
        auto synced = filesystem_->sync_directory(parent);
        if (!synced && !synced.error().is(filesystem::FileSystemErrorCode::Unsupported)) {
            return std::unexpected(from_filesystem_error(std::move(synced.error())));
        }
    }
    return {};
}

} // namespace litedb::core::meta
