#pragma once

#include <cstdint>

namespace litedb::protocol
{

inline constexpr std::uint32_t FrameHeaderMagic = 0x4C444250;  ///< LDBP - LiteDB Protocol

inline constexpr std::uint16_t ProtocolVersion = 1;            ///< 协议版本号

inline constexpr std::size_t MaxFrameSize = 16 * 1024 * 1024;  ///< 最大帧大小 16 MB

} // namespace litedb::protocol
