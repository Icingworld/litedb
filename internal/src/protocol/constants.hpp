#pragma once

#include <cstddef>
#include <cstdint>

namespace litedb::protocol
{

inline constexpr std::uint32_t FrameHeaderMagic = 0x4C444250;                 // 帧头魔数 - LDBP - LiteDB Protocol
inline constexpr std::uint16_t ProtocolVersion = 1;                           // 协议版本 - 1
inline constexpr std::size_t FrameHeaderSize = 32;                            // 帧头大小 - 32 字节
inline constexpr std::size_t MaxFrameSize = 16 * 1024 * 1024;                 // 最大帧大小 - 16MB
inline constexpr std::size_t MaxPayloadSize =
    MaxFrameSize - FrameHeaderSize;                                           // 最大负载大小

inline constexpr std::uint32_t DefaultMaxStringBytes = 1U * 1024U * 1024U;    // 默认最大字符串字节数
inline constexpr std::uint32_t DefaultMaxSqlBytes = 1U * 1024U * 1024U;       // 默认最大 SQL 字节数
inline constexpr std::uint32_t DefaultMaxColumns = 4U * 1024U;                // 默认最大列数
inline constexpr std::uint32_t DefaultMaxRows = 65536U;                       // 默认最大行数
inline constexpr std::uint32_t DefaultMaxValuesPerRow = 4096U;                // 默认最大每行值数
inline constexpr std::uint32_t DefaultMaxVectorElements =
    static_cast<std::uint32_t>(MaxPayloadSize / sizeof(double));              // 默认最大向量元素数

} // namespace litedb::protocol
