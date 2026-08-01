#include "core/logical_planner/statement/command/drop_database_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

DropDatabasePlan::DropDatabasePlan(
    std::optional<common::DatabaseId> database_id,
    std::string database_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : LogicalStatementPlan(LogicalStatementPlanKind::DropDatabase, location)
    , database_id_(database_id)
    , database_name_(std::move(database_name))
    , if_exists_(if_exists)
{
}

std::optional<common::DatabaseId> DropDatabasePlan::database_id() const noexcept
{
    return database_id_;
}

const std::string & DropDatabasePlan::database_name() const noexcept
{
    return database_name_;
}

bool DropDatabasePlan::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::planner::plan
