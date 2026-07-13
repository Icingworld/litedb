#include "core/io/byte_reader.hpp"

#include "core/io/io_error.hpp"
#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

std::expected<void, IoError> ByteReader::read_exact(std::span<std::byte> data)
{
    while (!data.empty()) {
        auto read = read_some(data);
        if (!read.has_value()) {
            return std::unexpected(read.error());
        }
        if (read.value() == 0) {
            return std::unexpected(make_io_error(
                IoErrorCode::UnexpectedEof,
                "unexpected end of binary data"
            ));
        }
        data = data.subspan(read.value());
    }
    return {};
}

} // namespace litedb::core::io

