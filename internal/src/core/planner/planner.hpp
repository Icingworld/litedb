#pragma once

#include <expected>
#include <memory>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/index/index_manager.hpp"
#include "core/planner/logical/logical_planner.hpp"
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
    Planner() noexcept;

    explicit Planner(const index::IndexManager * index_manager) noexcept;

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

private:
    // 后续元数据拆分后，可以把这个成员变量改为临时实例化，不是所有路径都需要使用这个计划器
    logical::LogicalPlanner logical_planner_;   ///< 逻辑计划器
};

} // namespace litedb::core::planner
