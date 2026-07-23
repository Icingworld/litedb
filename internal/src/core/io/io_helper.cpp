#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

IoError make_io_error(IoErrorCode code, std::string_view message)
{
    return IoError {code, message};
}

} // namespace litedb::core::io
