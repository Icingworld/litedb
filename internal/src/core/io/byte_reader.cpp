#include "core/io/byte_reader.hpp"

#include <cassert>

#include "core/io/io_error.hpp"
#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

std::expected<void, IoError> ByteReader::read_exact(std::span<std::byte> data)
{
    while (!data.empty()) {
        auto read = read_some(data);
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
        if (*read == 0) {
            return std::unexpected(
                make_io_error(IoErrorCode::UnexpectedEof, "unexpected end of binary data")
            );
        }
        assert(*read <= data.size());
        if (*read > data.size()) {
            return std::unexpected(make_io_error(
                IoErrorCode::InvalidData,
                "byte reader returned more bytes than requested"
            ));
        }
        data = data.subspan(*read);
    }
    return {};
}

} // namespace litedb::core::io
