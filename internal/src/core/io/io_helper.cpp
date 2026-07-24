#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

IOError make_io_error(IOErrorCode code, const std::string & message)
{
    return IOError {code, message};
}

} // namespace litedb::core::io
