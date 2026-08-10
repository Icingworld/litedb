#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "core/binder/bound/bound_assignment.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 UPDATE 语句
class BoundUpdateStatement final : public BoundStatement
{
public:
    BoundUpdateStatement(
        common::CollectionId collection_id,
        std::vector<BoundAssignment> assignments,
        std::unique_ptr<BoundExpression> where
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取赋值列表
    [[nodiscard]]
    const std::vector<BoundAssignment> & assignments() const noexcept;

    // 获取赋值列表所有权
    [[nodiscard]]
    std::vector<BoundAssignment> take_assignments() noexcept;

    // 获取条件表达式
    [[nodiscard]]
    std::optional<const BoundExpression &> where() const noexcept;

    // 获取条件表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

private:
    common::CollectionId collection_id_;
    std::vector<BoundAssignment> assignments_;
    std::unique_ptr<BoundExpression> where_;
};

} // namespace litedb::core::binder::bound
