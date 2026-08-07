#include "protocol/codec.hpp"

#include <limits>
#include <string_view>
#include <utility>

#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"

namespace litedb::protocol
{

namespace
{

ProtocolError make_error(ProtocolErrorCode code, std::string_view message)
{
    return ProtocolError {code, message};
}

/**
 * @brief 读取帧头
 * @param bytes 字节序列
 * @return 读取到的帧头
 */
std::expected<WireHeader, ProtocolError> read_wire_header(
    std::span<const std::byte> bytes
)
{
    // 验证字节序列大小
    if (bytes.size() < FrameHeaderSize) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnexpectedEnd,
            "frame header is shorter than 32 bytes"
        ));
    }

    // 从字节序列中截取前 32 个字节作为源字节序列
    core::io::BufferByteReader source {bytes.first(FrameHeaderSize)};
    core::io::BigEndianBinaryReader reader {
        source,
        core::io::BinaryDecodeLimits {
            .max_total_bytes = FrameHeaderSize,
            .max_string_bytes = 0,
        },
    };

    // 验证魔数
    auto magic = reader.read_u32();
    if (!magic) {
        return std::unexpected(std::move(magic.error()));
    }
    if (*magic != FrameHeaderMagic) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidMagic,
            "invalid frame magic"
        ));
    }

    // 读取帧大小
    auto frame_size = reader.read_u32();
    if (!frame_size) {
        return std::unexpected(std::move(frame_size.error()));
    }
    if (*frame_size < FrameHeaderSize) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidFrameSize,
            "frame size is smaller than the frame header"
        ));
    }
    if (*frame_size > MaxFrameSize) {
        return std::unexpected(make_error(
            ProtocolErrorCode::FrameTooLarge,
            "frame size exceeds the configured maximum"
        ));
    }

    // 验证协议版本号
    auto version = reader.read_u16();
    if (!version) {
        return std::unexpected(std::move(version.error()));
    }
    if (*version != ProtocolVersion) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnsupportedVersion,
            "unsupported protocol version"
        ));
    }

    // 读取帧头大小
    auto header_size = reader.read_u16();
    if (!header_size) {
        return std::unexpected(std::move(header_size.error()));
    }
    if (*header_size != FrameHeaderSize) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidHeaderSize,
            "unsupported frame header size"
        ));
    }

    // 读取消息类型
    auto kind = reader.read_u16();
    if (!kind) {
        return std::unexpected(std::move(kind.error()));
    }
    const auto message_kind = static_cast<MessageKind>(*kind);
    if (!is_known_message_kind(message_kind)) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidMessageKind,
            "invalid message kind"
        ));
    }

    // 读取标志
    auto flags = reader.read_u16();
    if (!flags) {
        return std::unexpected(std::move(flags.error()));
    }
    if (*flags != 0) {
        // 协议版本 1 不支持标志位
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidFlags,
            "frame flags are not supported by protocol version 1"
        ));
    }

    // 读取请求 ID
    auto request_id = reader.read_u64();
    if (!request_id) {
        return std::unexpected(std::move(request_id.error()));
    }

    // 读取保留字段
    auto reserved = reader.read_u64();
    if (!reserved) {
        return std::unexpected(std::move(reserved.error()));
    }
    if (*reserved != 0) {
        // 保留字段必须为 0
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidReservedField,
            "reserved frame field must be zero"
        ));
    }

    return WireHeader {
        .header = FrameHeader {
            .version = *version,
            .kind = message_kind,
            .flags = *flags,
            .request_id = *request_id,
        },
        .frame_size = *frame_size,
    };
}

} // namespace

std::expected<std::vector<std::byte>, ProtocolError> encode_frame(const Frame & frame)
{
    // 验证协议版本号
    if (frame.header.version != ProtocolVersion) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnsupportedVersion,
            "unsupported protocol version"
        ));
    }
    // 验证消息类型
    if (!is_known_message_kind(frame.header.kind)) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidMessageKind,
            "invalid message kind"
        ));
    }
    // 验证标志
    if (frame.header.flags != 0) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidFlags,
            "frame flags are not supported"
        ));
    }
    // 验证负载大小
    if (frame.payload.size() > MaxPayloadSize
        || frame.payload.size() > std::numeric_limits<std::uint32_t>::max() - FrameHeaderSize) {
        return std::unexpected(make_error(
            ProtocolErrorCode::FrameTooLarge,
            "frame payload exceeds the configured maximum"
        ));
    }

    const auto frame_size = static_cast<std::uint32_t>(FrameHeaderSize + frame.payload.size());
    core::io::BufferByteWriter buffer {MaxFrameSize};
    core::io::BigEndianBinaryWriter writer {buffer};

    // 写入魔数
    if (auto result = writer.write_u32(FrameHeaderMagic); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入帧大小
    if (auto result = writer.write_u32(frame_size); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入协议版本
    if (auto result = writer.write_u16(frame.header.version); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入帧头大小
    if (auto result = writer.write_u16(
        static_cast<std::uint16_t>(FrameHeaderSize)
    ); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入消息类型
    if (auto result = writer.write_u16(
        static_cast<std::uint16_t>(frame.header.kind)
    ); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入标志
    if (auto result = writer.write_u16(frame.header.flags); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入请求 ID
    if (auto result = writer.write_u64(frame.header.request_id); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入保留字段
    if (auto result = writer.write_u64(0); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 写入负载
    if (auto result = buffer.write_bytes(frame.payload); !result) {
        return std::unexpected(std::move(result.error()));
    }

    return buffer.take_bytes();
}

std::expected<WireHeader, ProtocolError> decode_frame_header(std::span<const std::byte> bytes)
{
    return read_wire_header(bytes);
}

std::expected<Frame, ProtocolError> decode_frame(std::span<const std::byte> bytes)
{
    // 读取线帧头
    auto header = read_wire_header(bytes);
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    if (bytes.size() < header->frame_size) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnexpectedEnd,
            "frame payload is truncated"
        ));
    }
    if (bytes.size() > header->frame_size) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidFrameSize,
            "frame has trailing bytes"
        ));
    }

    std::vector<std::byte> payload {
        bytes.begin() + static_cast<std::ptrdiff_t>(FrameHeaderSize),
        bytes.end(),
    };
    return Frame {
        .header = header->header,
        .payload = std::move(payload)
    };
}

bool is_known_message_kind(MessageKind kind) noexcept
{
    switch (kind) {
    case MessageKind::HelloRequest:
        [[fallthrough]];
    case MessageKind::HelloResponse:
        [[fallthrough]];
    case MessageKind::CloseRequest:
        [[fallthrough]];
    case MessageKind::ExecuteSqlRequest:
        [[fallthrough]];
    case MessageKind::ExecuteSqlResponse:
        [[fallthrough]];
    case MessageKind::CancelRequest:
        [[fallthrough]];
    case MessageKind::PingRequest:
        [[fallthrough]];
    case MessageKind::PongResponse:
        [[fallthrough]];
    case MessageKind::ErrorResponse:
        return true;
    }
    return false;
}

} // namespace litedb::protocol
