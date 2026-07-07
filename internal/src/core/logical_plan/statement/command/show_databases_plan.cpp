#include "core/logical_plan/statement/command/show_databases_plan.hpp"

namespace litedb::core::planner::plan
{

ShowDatabasesPlan::ShowDatabasesPlan(parser::ast::AstNodeLocation location)
    : StatementPlan(StatementPlanKind::ShowDatabases, location)
{
}

} // namespace litedb::core::planner::plan
