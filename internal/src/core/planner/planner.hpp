#pragma once

#include <expected>
#include <memory>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/planner/logical/logical_planner.hpp"
#include "core/planner/logical/planner_error.hpp"
#include "core/planner/statement/statement_plan.hpp"

namespace litedb::core::planner
{

/**
 * @brief 计划器
 */
class Planner
{
public:
    /**
     * @brief 计划语句
     * @param statement 语句
     * @return 计划结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<StatementPlan>, logical::PlannerError> plan(
        std::unique_ptr<binder::bound::BoundStatement> statement
    ) const;

private:
    logical::LogicalPlanner logical_planner_;   ///< 逻辑计划器
};

} // namespace litedb::core::planner
