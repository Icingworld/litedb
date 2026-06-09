#include "core/planner/statement/show_databases_plan.hpp"

namespace litedb::core::planner
{

ShowDatabasesPlan::ShowDatabasesPlan(parser::ast::AstNodeLocation location)
    : StatementPlan(StatementPlanKind::ShowDatabases, location)
{
}

} // namespace litedb::core::planner
