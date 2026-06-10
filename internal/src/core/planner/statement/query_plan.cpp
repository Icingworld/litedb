#include "core/planner/statement/query_plan.hpp"

#include <utility>

namespace litedb::core::planner
{

QueryPlan::QueryPlan(std::unique_ptr<logical::LogicalPlanNode> root, parser::ast::AstNodeLocation location)
    : StatementPlan(StatementPlanKind::Query, location)
    , root_(std::move(root))
{
}

const logical::LogicalPlanNode & QueryPlan::root() const noexcept
{
    return *root_;
}

} // namespace litedb::core::planner
