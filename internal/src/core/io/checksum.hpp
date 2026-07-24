#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace litedb::core::io
{

/**
 * @brief 计算 IEEE CRC32。
 */
[[nodiscard]]
std::uint32_t crc32(std::span<const std::byte> bytes) noexcept;

} // namespace litedb::core::io
