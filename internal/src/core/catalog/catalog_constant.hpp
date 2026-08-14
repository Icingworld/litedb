#pragma once

#include <cstdint>
#include <cstddef>

namespace litedb::core::catalog
{

constexpr std::uint32_t CatalogMagic = 0x544d444c; // LDMT, retained for format compatibility

constexpr std::uint16_t CatalogVersion = 1;

constexpr std::uint16_t CatalogHeaderSize = 24;

constexpr std::uint64_t MaxPayloadSize = 64ULL * 1024ULL * 1024ULL; // 64 MB

constexpr std::size_t MaxStringSize = 1024ULL * 1024ULL; // 1 MB

constexpr std::uint32_t MaxEntryCount = 1'000'000U; // 1 Million

constexpr std::size_t MaxExpressionDepth = 2; // 表达式深度限制，当前最多只会出现一层嵌套

} // namespace litedb::core::catalog
