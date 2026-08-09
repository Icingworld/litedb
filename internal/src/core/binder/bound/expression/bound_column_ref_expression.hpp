#pragma once

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定列引用表达式
class BoundColumnRefExpression final : public BoundExpression
{
public:
    BoundColumnRefExpression(
        common::ColumnId column_id,
        std::size_t column_ordinal,
        common::LogicalType type
    );

public:
    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取列在集合中的序号
    [[nodiscard]]
    std::size_t column_ordinal() const noexcept;

private:
    common::ColumnId column_id_;
    std::size_t column_ordinal_;
};

} // namespace litedb::core::binder::bound
