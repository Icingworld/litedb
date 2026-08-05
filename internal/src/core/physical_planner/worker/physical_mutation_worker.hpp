#pragma once

#include <memory>

#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::logical_planner::plan
{

class DeletePlan;
class InsertPlan;
class UpdatePlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::physical_planner::plan
{

class PhysicalPlan;

} // namespace litedb::core::physical_planner::plan

namespace litedb::core::physical_planner
{

/**
 * @brief 变更类逻辑计划 lowering worker
 */
class PhysicalMutationWorker final
{
public:
    explicit PhysicalMutationWorker(const PhysicalPlannerContext & context) noexcept
        : context_(context)
    {
    }

public:
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_insert(
        logical_planner::plan::InsertPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_update(
        logical_planner::plan::UpdatePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_delete(
        logical_planner::plan::DeletePlan & logical_plan
    );

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
