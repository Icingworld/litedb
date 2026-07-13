#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

IoError make_io_error(IoErrorCode code, const std::string & message)
{
    return IoError { code, message };
}

} // namespace litedb::core::io
