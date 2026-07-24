#include "core/parser/parser_helper.hpp"

#include <cctype>
#include <string>
#include <algorithm>
#include <string_view>

namespace litedb::core::parser
{

std::string lower_ascii(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}

ParserError make_parser_error(ParserErrorCode code, TokenLocation location, std::string_view message)
{
    return ParserError {code, message, ParserErrorContext {location}};
}

} // namespace litedb::core::parser
