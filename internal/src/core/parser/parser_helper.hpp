#pragma once

#include <string>
#include <string_view>

#include "core/parser/parser_error.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

// 将字符串转换为小写 ASCII 字符串
[[nodiscard]]
std::string lower_ascii(std::string_view value);

// 创建解析器错误
[[nodiscard]]
ParserError
make_parser_error(ParserErrorCode code, TokenLocation location, std::string_view message);

} // namespace litedb::core::parser
