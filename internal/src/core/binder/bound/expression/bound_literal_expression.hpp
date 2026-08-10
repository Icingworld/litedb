#pragma once

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/value.hpp"

namespace litedb::core::binder::bound
{

// 绑定常量表达式
// 该表达式被表达式评估器调用，但目前评估器
// 不会修改该表达式，而是直接转移整个常量表达式节点，
// 因此不为该表达式提供 take_value() 方法
class BoundLiteralExpression final : public BoundExpression
{
public:
    BoundLiteralExpression(common::LogicalType type, common::Value value);

public:
    // 获取常量值
    [[nodiscard]]
    const common::Value & value() const noexcept;

private:
    common::Value value_;
};

} // namespace litedb::core::binder::bound
