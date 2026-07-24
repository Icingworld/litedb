#include "core/io/checksum.hpp"

namespace litedb::core::io
{

std::uint32_t crc32(std::span<const std::byte> bytes) noexcept
{
    std::uint32_t checksum = 0xffffffffU;
    for (const auto byte : bytes) {
        checksum ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(checksum & 1U)));
            checksum = (checksum >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return ~checksum;
}

} // namespace litedb::core::io
