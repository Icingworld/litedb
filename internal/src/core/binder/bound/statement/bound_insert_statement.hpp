#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 INSERT 语句
class BoundInsertStatement final : public BoundStatement
{
public:
    BoundInsertStatement(
        common::CollectionId collection_id,
        std::vector<std::unique_ptr<BoundExpression>> values
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取值列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & values() const noexcept;

    // 获取值列表所有权
    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_values() noexcept;

private:
    common::CollectionId collection_id_;
    std::vector<std::unique_ptr<BoundExpression>> values_;
};

} // namespace litedb::core::binder::bound
