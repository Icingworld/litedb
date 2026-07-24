#include "core/storage/value_codec.hpp"

#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/io/io_helper.hpp"

namespace litedb::core::storage
{

namespace
{

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

std::expected<void, io::IoError> write_kind(
    io::BinaryWriter & writer,
    EncodedValueKind kind
)
{
    return writer.write_u8(static_cast<std::uint8_t>(kind));
}

} // namespace

std::expected<void, io::IoError> write_value(
    io::BinaryWriter & writer,
    const common::Value & value
)
{
    return std::visit(
        [&writer](const auto & data) -> std::expected<void, io::IoError> {
            using T = std::decay_t<decltype(data)>;
            EncodedValueKind kind {};
            if constexpr (std::is_same_v<T, common::NullValue>) {
                return write_kind(writer, EncodedValueKind::Null);
            } else if constexpr (std::is_same_v<T, bool>) {
                kind = EncodedValueKind::Boolean;
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                kind = EncodedValueKind::Integer;
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                kind = EncodedValueKind::BigInt;
            } else if constexpr (std::is_same_v<T, float>) {
                kind = EncodedValueKind::Float;
            } else if constexpr (std::is_same_v<T, double>) {
                kind = EncodedValueKind::Double;
            } else if constexpr (std::is_same_v<T, std::string>) {
                kind = EncodedValueKind::String;
            } else {
                static_assert(std::is_same_v<T, common::VectorValue>);
                kind = EncodedValueKind::Vector;
                if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
                    return std::unexpected(io::make_io_error(
                        io::IoErrorCode::ValueTooLarge,
                        "vector is too large to encode"
                    ));
                }
            }

            if (auto result = write_kind(writer, kind); !result) {
                return std::unexpected(std::move(result.error()));
            }
            if constexpr (std::is_same_v<T, common::NullValue>) {
                return {};
            } else if constexpr (std::is_same_v<T, bool>) {
                return writer.write_u8(data ? 1U : 0U);
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                return writer.write_i32(data);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return writer.write_i64(data);
            } else if constexpr (std::is_same_v<T, float>) {
                return writer.write_f32(data);
            } else if constexpr (std::is_same_v<T, double>) {
                return writer.write_f64(data);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return writer.write_string(data);
            } else {
                if (auto count = writer.write_u32(static_cast<std::uint32_t>(data.size())); !count) {
                    return std::unexpected(std::move(count.error()));
                }
                for (const auto element : data) {
                    if (auto result = writer.write_f64(element); !result) {
                        return std::unexpected(std::move(result.error()));
                    }
                }
                return {};
            }
        },
        value.data()
    );
}

std::expected<common::Value, io::IoError> read_value(
    io::BinaryReader & reader,
    ValueDecodeLimits limits
)
{
    auto kind_byte = reader.read_u8();
    if (!kind_byte) {
        return std::unexpected(std::move(kind_byte.error()));
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
            return std::unexpected(io::make_io_error(
                io::IoErrorCode::InvalidData,
                "boolean value must be encoded as zero or one"
            ));
        }
        return common::Value {*value != 0};
    }
    case EncodedValueKind::Integer: {
        auto value = reader.read_i32();
        if (!value) return std::unexpected(std::move(value.error()));
        return common::Value {*value};
    }
    case EncodedValueKind::BigInt: {
        auto value = reader.read_i64();
        if (!value) return std::unexpected(std::move(value.error()));
        return common::Value {*value};
    }
    case EncodedValueKind::Float: {
        auto value = reader.read_f32();
        if (!value) return std::unexpected(std::move(value.error()));
        return common::Value {*value};
    }
    case EncodedValueKind::Double: {
        auto value = reader.read_f64();
        if (!value) return std::unexpected(std::move(value.error()));
        return common::Value {*value};
    }
    case EncodedValueKind::String: {
        auto value = reader.read_string();
        if (!value) return std::unexpected(std::move(value.error()));
        return common::Value {std::move(*value)};
    }
    case EncodedValueKind::Vector: {
        auto count = reader.read_u32();
        if (!count) {
            return std::unexpected(std::move(count.error()));
        }
        if (*count > limits.max_vector_elements) {
            return std::unexpected(io::make_io_error(
                io::IoErrorCode::ValueTooLarge,
                "vector exceeds the configured element limit"
            ));
        }
        if (*count > reader.remaining_bytes() / sizeof(double)) {
            return std::unexpected(io::make_io_error(
                io::IoErrorCode::UnexpectedEof,
                "vector length exceeds the remaining binary data"
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

    return std::unexpected(io::make_io_error(
        io::IoErrorCode::InvalidData,
        "invalid encoded value kind"
    ));
}

} // namespace litedb::core::storage
