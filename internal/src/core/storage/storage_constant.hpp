#pragma once

#include <cstddef>
#include <cstdint>

namespace litedb::core::storage
{

inline constexpr std::size_t StoragePageSize = 4096; // 存储页大小

inline constexpr std::size_t StoragePageHeaderSize = 24; // 存储页头大小

inline constexpr std::size_t StorageSlotSize = 8; // 存储槽大小

inline constexpr std::uint16_t StorageFormatVersion = 1;

inline constexpr std::size_t MaxEncodedRecordSize =
    StoragePageSize - StoragePageHeaderSize - StorageSlotSize; // 最大编码记录大小

} // namespace litedb::core::storage
