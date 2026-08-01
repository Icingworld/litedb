#include "core/logical_planner/statement/command/show_databases_plan.hpp"

namespace litedb::core::planner::plan
{

ShowDatabasesPlan::ShowDatabasesPlan(parser::ast::AstNodeLocation location)
    : LogicalStatementPlan(LogicalStatementPlanKind::ShowDatabases, location)
{
}

} // namespace litedb::core::planner::plan
