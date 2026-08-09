#pragma once

#include <memory>
#include <optional>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 DELETE 语句
class BoundDeleteStatement final : public BoundStatement
{
public:
    BoundDeleteStatement(
        common::CollectionId collection_id,
        std::unique_ptr<BoundExpression> where
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取条件表达式
    [[nodiscard]]
    std::optional<const BoundExpression &> where() const noexcept;

    // 获取条件表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

private:
    common::CollectionId collection_id_;
    std::unique_ptr<BoundExpression> where_;
};

} // namespace litedb::core::binder::bound
