#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_assignment.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 UPDATE 语句
 */
class BoundUpdateStatement final : public BoundStatement
{
public:
    BoundUpdateStatement(
        common::CollectionId collection_id,
        std::vector<BoundAssignment> assignments,
        std::unique_ptr<BoundExpression> where
    );

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取赋值列表
     * @return 赋值列表
     */
    [[nodiscard]]
    const std::vector<BoundAssignment> & assignments() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const BoundExpression * where() const noexcept;

    /**
     * @brief 获取赋值列表
     * @return 赋值列表
     */
    [[nodiscard]]
    std::vector<BoundAssignment> take_assignments() noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

private:
    common::CollectionId collection_id_;            // 集合 ID
    std::vector<BoundAssignment> assignments_;      // 赋值列表
    std::unique_ptr<BoundExpression> where_;        // 条件表达式
};

} // namespace litedb::core::binder::bound
