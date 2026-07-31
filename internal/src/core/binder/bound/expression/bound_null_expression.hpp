#pragma once

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 NULL 表达式
 */
class BoundNullExpression final : public BoundExpression
{
public:
    BoundNullExpression(common::LogicalType type);
};

} // namespace litedb::core::binder::bound
