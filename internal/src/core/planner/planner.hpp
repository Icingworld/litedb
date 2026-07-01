#pragma once

#include <expected>
#include <memory>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/index/index_manager.hpp"
#include "core/planner/planner_error.hpp"
#include "core/planner/plan/statement_plan.hpp"

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
    std::expected<std::unique_ptr<plan::StatementPlan>, PlannerError> plan(
        std::unique_ptr<binder::bound::BoundStatement> statement
    ) const;
};

} // namespace litedb::core::planner
