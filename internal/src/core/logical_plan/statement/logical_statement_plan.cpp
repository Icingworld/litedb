#include "core/logical_plan/statement/logical_statement_plan.hpp"

namespace litedb::core::planner::plan
{

LogicalStatementPlan::LogicalStatementPlan(LogicalStatementPlanKind kind, parser::ast::AstNodeLocation location) noexcept
    : kind_(kind)
    , location_(location)
{
}

LogicalStatementPlanKind LogicalStatementPlan::kind() const noexcept
{
    return kind_;
}

parser::ast::AstNodeLocation LogicalStatementPlan::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::planner::plan
