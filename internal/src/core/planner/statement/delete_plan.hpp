#pragma once

#include <memory>
#include <string>

#include "core/common/ids.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"
#include "core/planner/statement/statement_plan.hpp"

namespace litedb::core::planner
{

/**
 * @brief DELETE 语句计划
 */
class DeletePlan final : public StatementPlan
{
public:
    DeletePlan(
        std::unique_ptr<logical::LogicalPlanNode> input,
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取输入
     * @return 输入
     */
    [[nodiscard]]
    const logical::LogicalPlanNode & input() const noexcept;

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

private:
    std::unique_ptr<logical::LogicalPlanNode> input_;       ///< 输入
    common::DatabaseId database_id_;                        ///< 数据库ID
    common::CollectionId collection_id_;                    ///< 集合ID
    std::string collection_name_;                           ///< 集合名称
};

} // namespace litedb::core::planner
