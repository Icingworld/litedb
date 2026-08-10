#pragma once

#include <cstdint>
#include <expected>

#include "core/common/value.hpp"
#include "core/io/binary_io.hpp"

namespace litedb::core::storage
{

struct ValueDecodeLimits
{
    std::uint32_t max_vector_elements;
};

[[nodiscard]]
std::expected<void, io::IoError> write_value(
    io::LittleEndianBinaryWriter & writer,
    const common::Value & value
);

[[nodiscard]]
std::expected<common::Value, io::IoError> read_value(
    io::LittleEndianBinaryReader & reader,
    ValueDecodeLimits limits
);

} // namespace litedb::core::storage
