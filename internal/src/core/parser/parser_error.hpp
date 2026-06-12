#pragma once

#include <string>

#include "core/parser/token.hpp"

namespace litedb::core::parser
{

/**
 * @brief 解析器错误码
 */
enum class ParserErrorCode
{
    EmptyStatement,        ///< 空输入
    LexicalError,          ///< 词法错误
    UnexpectedStatement,   ///< 不支持的语句开始
    UnexpectedToken,       ///< 在有效解析边界后出现意外的 Token
    ExpectedToken,         ///< 期望特定的 Token
    ExpectedIdentifier,    ///< 期望标识符
    ExpectedExpression,    ///< 期望表达式
    ExpectedLiteral,       ///< 期望字面量
    ExpectedDataType,      ///< 期望数据类型
    EmptyList,             ///< 期望非空列表
    InvalidInteger,        ///< 无效的整数字面量
    UnsupportedSyntax,     ///< 识别但不支持的语法
};

/**
 * @brief 解析器错误
 */
struct ParserError
{
    ParserErrorCode code;       ///< 错误码
    TokenLocation location;     ///< 错误位置
    std::string message;        ///< 错误消息
};

} // namespace litedb::core::parser
