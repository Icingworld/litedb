#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"
#include "core/logical_plan/statement/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief INSERT 语句计划
 */
class InsertPlan final : public StatementPlan
{
public:
    InsertPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundColumn> columns,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
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
     * @brief 获取列
     * @return 列
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundColumn> & columns() const noexcept;

    /**
     * @brief 获取值
     * @return 值
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & values() const noexcept;

private:
    common::DatabaseId database_id_;                                            ///< 数据库 ID
    common::CollectionId collection_id_;                                        ///< 集合 ID
    std::string collection_name_;                                               ///< 集合名称
    std::vector<binder::bound::BoundColumn> columns_;                           ///< 列
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;       ///< 值
};

} // namespace litedb::core::planner::plan
