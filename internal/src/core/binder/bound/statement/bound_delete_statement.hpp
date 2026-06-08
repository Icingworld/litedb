#pragma once

#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief DELETE 语句节点
 * @details 示例：DELETE FROM collection_name WHERE condition
 */
class BoundDeleteStatement final : public BoundStatement
{
public:
    BoundDeleteStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::unique_ptr<BoundExpression> where,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合ID
     * @return 集合ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const BoundExpression * where() const noexcept;

private:
    common::DatabaseId database_id_;                ///< 数据库ID
    common::CollectionId collection_id_;            ///< 集合ID
    std::string collection_name_;                   ///< 集合名称
    std::unique_ptr<BoundExpression> where_;        ///< 条件表达式
};

} // namespace litedb::core::binder::bound
