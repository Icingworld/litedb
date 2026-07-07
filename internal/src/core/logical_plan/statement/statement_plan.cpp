#include "core/logical_plan/statement/statement_plan.hpp"

namespace litedb::core::planner::plan
{

StatementPlan::StatementPlan(StatementPlanKind kind, parser::ast::AstNodeLocation location) noexcept
    : kind_(kind)
    , location_(location)
{
}

StatementPlanKind StatementPlan::kind() const noexcept
{
    return kind_;
}

parser::ast::AstNodeLocation StatementPlan::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::planner::plan
