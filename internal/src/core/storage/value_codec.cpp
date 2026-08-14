#include "core/storage/value_codec.hpp"
#include "storage_error.hpp"

#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

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
std::expected<void, StorageError> write_kind(
    io::LittleEndianBinaryWriter & writer,
    EncodedValueKind kind
)
{
    return writer.write_u8(static_cast<std::uint8_t>(kind));
}

} // namespace

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

} // namespace litedb::core::storage
