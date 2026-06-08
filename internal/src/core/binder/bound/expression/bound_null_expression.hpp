#pragma once

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief NULL 表达式节点
 * @details 示例：NULL
 */
class BoundNullExpression final : public BoundExpression
{
public:
    BoundNullExpression(common::LogicalType type, parser::ast::AstNodeLocation location);
};

} // namespace litedb::core::binder::bound
