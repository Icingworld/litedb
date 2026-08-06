#include "core/logical_planner/plan/command/create_database_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

CreateDatabasePlan::CreateDatabasePlan(
    std::optional<std::string> database_name
)
    : LogicalPlan(LogicalPlanKind::CreateDatabase)
    , database_name_(std::move(database_name))
{
}

const std::optional<std::string> &
CreateDatabasePlan::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::logical_planner::plan
