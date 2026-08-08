#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

// 解析器错误码
enum class ParserErrorCode : std::uint8_t
{
    EmptyStatement = 0,        // 空输入
    LexicalError = 1,          // 词法错误
    UnexpectedStatement = 2,   // 不支持的语句开始
    UnexpectedToken = 3,       // 在有效解析边界后出现意外的 Token
    ExpectedToken = 4,         // 期望特定的 Token
    ExpectedIdentifier = 5,    // 期望标识符
    ExpectedExpression = 6,    // 期望表达式
    ExpectedLiteral = 7,       // 期望字面量
    ExpectedDataType = 8,      // 期望数据类型
    EmptyList = 9,             // 期望非空列表
    InvalidInteger = 10,       // 无效的整数字面量
    UnsupportedSyntax = 11,    // 识别但不支持的语法
};

// 解析器错误上下文
struct ParserErrorContext
{
    TokenLocation location;     // 错误位置
};

using ParserError = error::Error;

} // namespace litedb::core::parser

namespace litedb::core::error
{

template <>
struct ErrorTraits<parser::ParserErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Parser;
};

} // namespace litedb::core::error
