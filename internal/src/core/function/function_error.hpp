#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数错误代码
 */
enum class FunctionErrorCode : std::uint8_t
{
    InvalidArgument,        ///< 无效参数
    InvalidType,            ///< 无效类型
};

/**
 * @brief 函数错误
 */
struct FunctionErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 位置
};

using FunctionError = error::Error;

} // namespace litedb::core::function

namespace litedb::core::error
{
template <>
struct ErrorTraits<function::FunctionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Function;
};
} // namespace litedb::core::error
