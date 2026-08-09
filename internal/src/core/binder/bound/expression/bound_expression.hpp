#pragma once

#include <cstdint>

#include "core/common/logical_type.hpp"

namespace litedb::core::binder::bound
{

// 绑定表达式类型
enum class BoundExpressionKind : std::uint8_t
{
    Literal,
    Null,
    ColumnRef,
    Unary,
    Binary,
    Vector,
    Function,
    In,
    Between,
    Like,
    Cast,
};

// 绑定表达式
class BoundExpression
{
public:
    BoundExpression(const BoundExpression &) = delete;

    BoundExpression & operator=(const BoundExpression &) = delete;

    BoundExpression(BoundExpression &&) noexcept = default;

    BoundExpression & operator=(BoundExpression &&) noexcept = default;

    virtual ~BoundExpression() noexcept = default;

protected:
    BoundExpression(BoundExpressionKind kind, common::LogicalType type) noexcept;

public:
    // 获取绑定表达式类型
    [[nodiscard]]
    BoundExpressionKind kind() const noexcept;

    // 获取逻辑类型
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

private:
    BoundExpressionKind kind_;
    common::LogicalType type_;
};

} // namespace litedb::core::binder::bound
