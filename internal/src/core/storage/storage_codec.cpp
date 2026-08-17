#include "core/storage/storage_codec.hpp"

#include "core/common/record.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/binary_io.hpp"
#include "core/storage/storage_constant.hpp"

namespace litedb::core::storage
{

namespace
{

// 编码值类型
enum class EncodedValueKind : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    BigInt = 3,
    Float = 4,
    Double = 5,
    String = 6,
    Vector = 7,
};

// 写入值类型
[[nodiscard]]
std::expected<void, StorageError> write_kind(
    io::LittleEndianBinaryWriter & writer,
    EncodedValueKind kind
)
{
    return writer.write_u8(static_cast<std::uint8_t>(kind));
}

// 写入值
[[nodiscard]]
std::expected<void, StorageError> write_value(
    io::LittleEndianBinaryWriter & writer,
    const common::Value & value
)
{
    return std::visit(
        [&writer](const auto & data) -> std::expected<void, StorageError> {
            using T = std::decay_t<decltype(data)>;

            if constexpr (std::is_same_v<T, common::NullValue>) {
                return write_kind(writer, EncodedValueKind::Null);
            } else if constexpr (std::is_same_v<T, bool>) {
                if (auto kind = write_kind(writer, EncodedValueKind::Boolean); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                return writer.write_u8(data ? 1U : 0U);
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                if (auto kind = write_kind(writer, EncodedValueKind::Integer); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                return writer.write_i32(data);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                if (auto kind = write_kind(writer, EncodedValueKind::BigInt); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                return writer.write_i64(data);
            } else if constexpr (std::is_same_v<T, float>) {
                if (auto kind = write_kind(writer, EncodedValueKind::Float); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                return writer.write_f32(data);
            } else if constexpr (std::is_same_v<T, double>) {
                if (auto kind = write_kind(writer, EncodedValueKind::Double); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                return writer.write_f64(data);
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (auto kind = write_kind(writer, EncodedValueKind::String); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                return writer.write_string(data);
            } else if constexpr (std::is_same_v<T, common::VectorValue>) {
                // 向量元素在不同的模块中都有检查，目前还没有统一的长度限制规范
                // 这里暂时使用 std::uint32_t 作为最大值，后续会进行统一
                if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::ValueTooLarge,
                        "vector is too large to encode",
                        {
                            .operation = StorageOperation::Encode,
                        }
                    ));
                }
                if (auto kind = write_kind(writer, EncodedValueKind::Vector); !kind) {
                    return std::unexpected(std::move(kind.error()));
                }
                if (auto count = writer.write_u32(static_cast<std::uint32_t>(data.size())); !count) {
                    return std::unexpected(std::move(count.error()));
                }
                for (const auto element : data) {
                    // 向量元素使用 double 类型存储
                    if (auto result = writer.write_f64(element); !result) {
                        return std::unexpected(std::move(result.error()));
                    }
                }
                return {};
            } else {
                static_assert(false, "invalid value type");
            }
        },
        value.data()
    );
}

// 值解码限制
struct ValueDecodeLimits
{
    std::uint32_t max_vector_elements; // 最大向量元素数
};

// 读取值
[[nodiscard]]
std::expected<common::Value, StorageError> read_value(
    io::LittleEndianBinaryReader & reader,
    ValueDecodeLimits limits
)
{
    auto kind_byte = reader.read_u8();
    if (!kind_byte) {
        return std::unexpected(std::move(kind_byte.error()));
    }
    if (*kind_byte > static_cast<std::uint8_t>(EncodedValueKind::Vector)) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidData,
            "invalid encoded value kind",
            {
                .operation = StorageOperation::Decode,
            }
        ));
    }

    switch (static_cast<EncodedValueKind>(*kind_byte)) {
    case EncodedValueKind::Null:
        return common::Value::null();
    case EncodedValueKind::Boolean: {
        auto value = reader.read_u8();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        if (*value > 1) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidData,
                "boolean value must be encoded as zero or one",
                {
                    .operation = StorageOperation::Decode,
                }
            ));
        }
        return common::Value {*value == 1};
    }
    case EncodedValueKind::Integer: {
        auto value = reader.read_i32();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::BigInt: {
        auto value = reader.read_i64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::Float: {
        auto value = reader.read_f32();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::Double: {
        auto value = reader.read_f64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::String: {
        auto value = reader.read_string();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {std::move(*value)};
    }
    case EncodedValueKind::Vector: {
        auto count = reader.read_u32();
        if (!count) {
            return std::unexpected(std::move(count.error()));
        }
        if (*count > limits.max_vector_elements) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::ValueTooLarge,
                "vector exceeds the configured element limit",
                {
                    .operation = StorageOperation::Decode,
                }
            ));
        }
        if (*count > reader.remaining_bytes() / sizeof(double)) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::ResourceLimitExceeded,
                "vector length exceeds the remaining binary data",
                {
                    .operation = StorageOperation::Decode,
                }
            ));
        }
        common::VectorValue values;
        values.reserve(*count);
        for (std::uint32_t index = 0; index < *count; ++index) {
            auto value = reader.read_f64();
            if (!value) {
                return std::unexpected(std::move(value.error()));
            }
            values.push_back(*value);
        }
        return common::Value {std::move(values)};
    }
    }

    return std::unexpected(make_storage_error(
        StorageErrorCode::InvalidData,
        "invalid encoded value kind",
        {
            .operation = StorageOperation::Decode,
        }
    ));
}

} // namespace

std::expected<std::vector<std::byte>, StorageError> encode_record(const common::Record & record)
{
    io::BufferByteWriter bytes {MaxEncodedRecordSize};
    io::LittleEndianBinaryWriter writer {bytes};

    // 写入记录 ID
    if (auto result = writer.write_u64(record.id); !result) {
        return std::unexpected(std::move(result.error()));
    }

    // 写入记录数据数量
    if (record.data.values.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::RecordTooLarge,
            "Record has too many values",
            {
                .operation = StorageOperation::Encode,
                .record_id = record.id,
            }
        ));
    }
    if (auto result = writer.write_u32(static_cast<std::uint32_t>(record.data.values.size())); !result) {
        return std::unexpected(std::move(result.error()));
    }

    // 写入记录数据
    for (const auto & value : record.data.values) {
        if (auto result = write_value(writer, value); !result) {
            return std::unexpected(std::move(result.error()));
        }
    }

    return bytes.bytes();
}

std::expected<common::Record, StorageError> decode_record(std::span<const std::byte> bytes)
{
    if (bytes.size() > MaxEncodedRecordSize) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::RecordTooLarge,
            "Encoded record exceeds the storage page limit",
            {
                .operation = StorageOperation::Decode,
            }
        ));
    }

    io::BufferByteReader source {bytes};
    io::LittleEndianBinaryReader reader {
        source,
        {
            .max_total_bytes = bytes.size(),
            .max_string_bytes = static_cast<std::uint32_t>(bytes.size()),
        }
    };

    // 读取记录 ID
    auto id = reader.read_u64();
    if (!id) {
        return std::unexpected(std::move(id.error()));
    }
    if (*id == 0) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid record id",
            {
                .operation = StorageOperation::Decode,
            }
        ));
    }

    // 读取记录数据数量
    auto count = reader.read_u32();
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    // 最小大小的结构为:
    // std::uint32_t kind: 1 byte
    // 最小大小为 4 bytes
    constexpr std::size_t min_value_bytes = sizeof(std::uint8_t);
    if (*count > reader.remaining_bytes() / min_value_bytes) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Record data count exceeds the remaining binary data",
            {
                .operation = StorageOperation::Decode,
                .record_id = *id,
            }
        ));
    }

    // 读取记录数据
    common::RecordData data;
    data.values.reserve(*count);
    for (std::uint32_t index = 0; index < *count; ++index) {
        // 读取值
        // 每一次循环，剩余字节预算都会减少
        auto value = read_value(reader, {
            .max_vector_elements =
                static_cast<std::uint32_t>(reader.remaining_bytes() / sizeof(double)),
        });
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        data.values.push_back(std::move(*value));
    }

    return common::Record {*id, std::move(data)};
}

} // namespace litedb::core::storage
