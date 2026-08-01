#include "core/physical_planner/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

PhysicalStatementPlan::PhysicalStatementPlan(
    PhysicalStatementPlanKind kind,
    parser::ast::AstNodeLocation location
) noexcept
    : kind_(kind)
    , location_(location)
{
}

PhysicalStatementPlanKind PhysicalStatementPlan::kind() const noexcept
{
    return kind_;
}

parser::ast::AstNodeLocation PhysicalStatementPlan::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::physical_plan
