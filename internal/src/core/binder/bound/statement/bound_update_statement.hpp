#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 更新语句节点
 * @details 用于表示 column = value 的赋值操作
 */
struct BoundAssignment
{
    BoundColumn column;                             ///< 列
    std::unique_ptr<BoundExpression> value;         ///< 值
};

/**
 * @brief 更新语句节点
 * @details 示例：UPDATE collection_name SET column1 = value1, column2 = value2, ... WHERE condition
 */
class BoundUpdateStatement final : public BoundStatement
{
public:
    BoundUpdateStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<BoundAssignment> assignments,
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

    [[nodiscard]]
    std::vector<BoundAssignment> take_assignments() noexcept;

    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

private:
    common::DatabaseId database_id_;                ///< 数据库ID
    common::CollectionId collection_id_;            ///< 集合ID
    std::string collection_name_;                   ///< 集合名称
    std::vector<BoundAssignment> assignments_;      ///< 赋值列表
    std::unique_ptr<BoundExpression> where_;        ///< 条件表达式
};

} // namespace litedb::core::binder::bound
