#pragma once

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/value.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定常量表达式
 */
class BoundLiteralExpression final : public BoundExpression
{
public:
    BoundLiteralExpression(
        common::LogicalType type,
        common::Value value
    );

public:
    /**
     * @brief 获取常量值
     * @return 常量值
     */
    [[nodiscard]]
    const common::Value & value() const noexcept;

private:
    common::Value value_;     ///< 常量值
};

} // namespace litedb::core::binder::bound
