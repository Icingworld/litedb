#include "core/logical_plan/statement/command/use_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

UsePlan::UsePlan(common::DatabaseId database_id, std::string database_name, parser::ast::AstNodeLocation location)
    : StatementPlan(StatementPlanKind::Use, location)
    , database_id_(database_id)
    , database_name_(std::move(database_name))
{
}

common::DatabaseId UsePlan::database_id() const noexcept
{
    return database_id_;
}

const std::string & UsePlan::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::planner::plan
