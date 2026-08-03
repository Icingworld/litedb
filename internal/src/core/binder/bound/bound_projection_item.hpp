#pragma once

#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 投影项
 */
struct BoundProjectionItem
{
    std::unique_ptr<BoundExpression> expression;    ///< 投影表达式
    std::string output_name;                        ///< 投影输出名称
};

} // namespace litedb::core::binder::bound
