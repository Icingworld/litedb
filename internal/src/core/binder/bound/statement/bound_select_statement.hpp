#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "core/binder/bound/bound_order_by_item.hpp"
#include "core/binder/bound/bound_projection_item.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 SELECT 语句
class BoundSelectStatement final : public BoundStatement
{
public:
    BoundSelectStatement(
        common::CollectionId collection_id,
        std::vector<BoundProjectionItem> projections,
        std::unique_ptr<BoundExpression> where,
        std::vector<BoundOrderByItem> order_by,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取选择列表
    [[nodiscard]]
    const std::vector<BoundProjectionItem> & projections() const noexcept;

    // 获取条件表达式
    [[nodiscard]]
    std::optional<const BoundExpression &> where() const noexcept;

    // 获取排序列表
    [[nodiscard]]
    const std::vector<BoundOrderByItem> & order_by() const noexcept;

    // 获取限制
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    // 获取偏移
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

    // 获取选择列表所有权
    [[nodiscard]]
    std::vector<BoundProjectionItem> take_projections() noexcept;

    // 获取条件表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

    // 获取排序列表所有权
    [[nodiscard]]
    std::vector<BoundOrderByItem> take_order_by() noexcept;

private:
    common::CollectionId collection_id_;
    std::vector<BoundProjectionItem> projections_;
    std::unique_ptr<BoundExpression> where_;
    std::vector<BoundOrderByItem> order_by_;
    std::optional<std::size_t> limit_;
    std::optional<std::size_t> offset_;
};

} // namespace litedb::core::binder::bound
