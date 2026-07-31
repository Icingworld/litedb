#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/types.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定二元表达式
 */
class BoundBinaryExpression final : public BoundExpression
{
public:
    BoundBinaryExpression(
        std::unique_ptr<BoundExpression> left,
        common::BinaryOperator op,
        std::unique_ptr<BoundExpression> right,
        common::LogicalType type
    );

public:
    /**
     * @brief 获取左操作数
     * @return 左操作数
     */
    [[nodiscard]]
    const BoundExpression & left() const noexcept;

    /**
     * @brief 获取二元操作符
     * @return 二元操作符
     */
    [[nodiscard]]
    common::BinaryOperator op() const noexcept;

    /**
     * @brief 获取右操作数
     * @return 右操作数
     */
    [[nodiscard]]
    const BoundExpression & right() const noexcept;

private:
    std::unique_ptr<BoundExpression> left_;     ///< 左操作数
    common::BinaryOperator op_;                 ///< 二元操作符
    std::unique_ptr<BoundExpression> right_;    ///< 右操作数
};

} // namespace litedb::core::binder::bound
