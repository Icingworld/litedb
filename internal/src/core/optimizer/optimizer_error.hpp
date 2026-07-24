#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::optimizer
{

/**
 * @brief 优化器错误码
 */
enum class OptimizerErrorCode : std::uint8_t
{
    InvalidArgument,    ///< 无效参数
};

/**
 * @brief 优化器错误
 */
struct OptimizerErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 错误位置
};

using OptimizerError = error::Error;

} // namespace litedb::core::optimizer

namespace litedb::core::error
{
template <>
struct ErrorTraits<optimizer::OptimizerErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Optimizer;
};
} // namespace litedb::core::error
