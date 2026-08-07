#include "protocol/codec.hpp"

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{

using namespace litedb::protocol;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::byte byte(unsigned int value)
{
    return static_cast<std::byte>(value);
}

void test_frame_roundtrip_and_golden_bytes()
{
    const Frame frame {
        .header = FrameHeader {
            .version = ProtocolVersion,
            .kind = MessageKind::PingRequest,
            .flags = 0,
            .request_id = 42,
        },
        .payload = {},
    };

    auto encoded = encode_frame(frame);
    require(encoded.has_value(), "frame should encode");
    const std::vector<std::byte> expected {
        byte(0x4c), byte(0x44), byte(0x42), byte(0x50),
        byte(0x00), byte(0x00), byte(0x00), byte(0x20),
        byte(0x00), byte(0x01), byte(0x00), byte(0x20),
        byte(0x02), byte(0x00), byte(0x00), byte(0x00),
        byte(0x00), byte(0x00), byte(0x00), byte(0x00),
        byte(0x00), byte(0x00), byte(0x00), byte(0x2a),
        byte(0x00), byte(0x00), byte(0x00), byte(0x00),
        byte(0x00), byte(0x00), byte(0x00), byte(0x00),
    };
    require(*encoded == expected, "frame golden bytes mismatch");

    auto decoded = decode_frame(*encoded);
    require(decoded.has_value(), "frame should decode");
    require(decoded->header.kind == MessageKind::PingRequest, "message kind mismatch");
    require(decoded->header.request_id == 42, "request id mismatch");
    require(decoded->payload.empty(), "payload should be empty");
}

void test_header_validation()
{
    auto encoded = encode_frame(Frame {
        .header = FrameHeader {.kind = MessageKind::ErrorResponse, .request_id = 7},
        .payload = {byte(1), byte(2), byte(3)},
    });
    require(encoded.has_value(), "error frame should encode");

    auto magic = *encoded;
    magic[0] = byte(0);
    auto invalid_magic = decode_frame(magic);
    require(!invalid_magic && invalid_magic.error().is(ProtocolErrorCode::InvalidMagic), "invalid magic was accepted");

    auto version = *encoded;
    version[9] = byte(2);
    auto invalid_version = decode_frame(version);
    require(!invalid_version && invalid_version.error().is(ProtocolErrorCode::UnsupportedVersion), "invalid version was accepted");

    auto header_size = *encoded;
    header_size[11] = byte(0x10);
    auto invalid_header_size = decode_frame(header_size);
    require(!invalid_header_size && invalid_header_size.error().is(ProtocolErrorCode::InvalidHeaderSize), "invalid header size was accepted");

    auto too_small = *encoded;
    too_small[7] = byte(0x1f);
    auto invalid_frame_size = decode_frame(too_small);
    require(!invalid_frame_size && invalid_frame_size.error().is(ProtocolErrorCode::InvalidFrameSize), "small frame size was accepted");

    auto too_large = *encoded;
    too_large[4] = byte(0x01);
    auto oversized = decode_frame(too_large);
    require(!oversized && oversized.error().is(ProtocolErrorCode::FrameTooLarge), "oversized frame was accepted");

    auto trailing = *encoded;
    trailing.push_back(byte(0));
    auto invalid_size = decode_frame(trailing);
    require(!invalid_size && invalid_size.error().is(ProtocolErrorCode::InvalidFrameSize), "trailing bytes were accepted");

    auto truncated = std::vector<std::byte> {encoded->begin(), encoded->begin() + 10};
    auto short_header = decode_frame_header(truncated);
    require(!short_header && short_header.error().is(ProtocolErrorCode::UnexpectedEnd), "short header was accepted");

    auto unknown_kind = *encoded;
    unknown_kind[12] = byte(0x12);
    unknown_kind[13] = byte(0x34);
    auto invalid_kind = decode_frame(unknown_kind);
    require(!invalid_kind && invalid_kind.error().is(ProtocolErrorCode::InvalidMessageKind), "unknown kind was accepted");

    auto flags = *encoded;
    flags[15] = byte(1);
    auto invalid_flags = decode_frame(flags);
    require(!invalid_flags && invalid_flags.error().is(ProtocolErrorCode::InvalidFlags), "nonzero flags were accepted");

    auto reserved = *encoded;
    reserved[31] = byte(1);
    auto invalid_reserved = decode_frame(reserved);
    require(!invalid_reserved && invalid_reserved.error().is(ProtocolErrorCode::InvalidReservedField), "nonzero reserved field was accepted");
}

void test_sparse_message_kinds()
{
    require(is_known_message_kind(MessageKind::HelloRequest), "hello kind should be known");
    require(is_known_message_kind(MessageKind::ErrorResponse), "error kind should be known");
    require(!is_known_message_kind(static_cast<MessageKind>(0x0004)), "gap kind should be unknown");
}

} // namespace

int main()
{
    try {
        test_frame_roundtrip_and_golden_bytes();
        test_header_validation();
        test_sparse_message_kinds();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
