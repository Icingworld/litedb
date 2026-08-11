#include "core/physical_planner/plan/command/create_database_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

CreateDatabasePlan::CreateDatabasePlan(std::optional<std::string> database_name)
    : PhysicalPlan(PhysicalPlanKind::CreateDatabase)
    , database_name_(std::move(database_name))
{}

std::optional<const std::string &> CreateDatabasePlan::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::physical_planner::plan