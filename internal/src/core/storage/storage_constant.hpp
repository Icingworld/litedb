#pragma once

#include <cstddef>
#include <cstdint>

namespace litedb::core::storage
{

inline constexpr std::size_t StoragePageSize = 4096;

inline constexpr std::size_t StoragePageHeaderSize = 24;

inline constexpr std::size_t StorageSlotSize = 8;

inline constexpr std::uint16_t StorageFormatVersion = 1;

} // namespace litedb::core::storage
