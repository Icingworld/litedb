#pragma once

#include <cstdint>
#include <expected>

#include "core/common/value.hpp"
#include "core/io/binary_io.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

// 值解码限制
struct ValueDecodeLimits
{
    std::uint32_t max_vector_elements; // 最大向量元素数
};

// 写入值
[[nodiscard]]
std::expected<void, StorageError> write_value(
    io::LittleEndianBinaryWriter & writer,
    const common::Value & value
);

// 读取值
[[nodiscard]]
std::expected<common::Value, StorageError> read_value(
    io::LittleEndianBinaryReader & reader,
    ValueDecodeLimits limits
);

} // namespace litedb::core::storage
