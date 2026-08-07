#pragma once

#include <memory>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 DELETE 语句
 */
class BoundDeleteStatement final : public BoundStatement
{
public:
    BoundDeleteStatement(
        common::CollectionId collection_id,
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
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const BoundExpression * where() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

private:
    common::CollectionId collection_id_;            // 集合 ID
    std::unique_ptr<BoundExpression> where_;        // 条件表达式
};

} // namespace litedb::core::binder::bound
