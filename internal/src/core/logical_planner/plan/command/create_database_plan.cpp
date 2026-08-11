#include "core/logical_planner/plan/command/create_database_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

CreateDatabasePlan::CreateDatabasePlan(std::optional<std::string> database_name)
    : LogicalPlan(LogicalPlanKind::CreateDatabase)
    , database_name_(std::move(database_name))
{}

std::optional<const std::string &> CreateDatabasePlan::database_name() const noexcept
{
    return database_name_;
}

std::optional<std::string> CreateDatabasePlan::take_database_name() noexcept
{
    return std::exchange(database_name_, std::nullopt);
}

} // namespace litedb::core::logical_planner::plan
