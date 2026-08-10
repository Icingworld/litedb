#include "core/io/checksum.hpp"

#include <array>

namespace litedb::core::io
{

namespace detail
{

inline constexpr std::uint32_t Crc32Polynomial = 0xedb88320U; // IEEE CRC32 多项式

// 在编译期生成 IEEE CRC32 查表
[[nodiscard]]
consteval std::array<std::uint32_t, 256> make_crc32_table() noexcept
{
    std::array<std::uint32_t, 256> table {};
    for (std::uint32_t index = 0; index < table.size(); ++index) {
        auto entry = index;
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask =
                static_cast<std::uint32_t>(-static_cast<std::int32_t>(entry & 1U));
            entry = (entry >> 1U) ^ (Crc32Polynomial & mask);
        }
        table[index] = entry;
    }
    return table;
}

// 编译期生成的 IEEE CRC32 查表
inline constexpr auto Crc32Table = make_crc32_table();

} // namespace detail

Crc32Calculator::Crc32Calculator() noexcept
    : checksum_(0xffffffffU)
{
}

void Crc32Calculator::update(std::span<const std::byte> data) noexcept
{
    for (const auto byte : data) {
        const auto index = static_cast<std::uint8_t>(checksum_ ^ static_cast<std::uint8_t>(byte));
        checksum_ = detail::Crc32Table[index] ^ (checksum_ >> 8U);
    }
}

std::uint32_t Crc32Calculator::value() const noexcept
{
    return ~checksum_;
}

std::uint32_t crc32(std::span<const std::byte> bytes) noexcept
{
    Crc32Calculator calculator;
    calculator.update(bytes);
    return calculator.value();
}

} // namespace litedb::core::io
