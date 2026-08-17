#pragma once

#include <expected>
#include <vector>
#include <cstddef>

#include "core/common/record.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

// 编码记录
std::expected<std::vector<std::byte>, StorageError> encode_record(const common::Record & record);

// 解码记录
std::expected<common::Record, StorageError> decode_record(std::span<const std::byte> bytes);

} // namespace litedb::core::storage
