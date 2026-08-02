#include "core/logical_planner/plan/query/query_plan.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::logical_planner::plan
{

QueryPlan::QueryPlan(std::unique_ptr<logical::LogicalPlanNode> root, parser::ast::AstNodeLocation location)
    : LogicalStatementPlan(LogicalStatementPlanKind::Query, location)
    , root_(std::move(root))
{
}

const logical::LogicalPlanNode & QueryPlan::root() const noexcept
{
    assert(root_ != nullptr);
    return *root_;
}

} // namespace litedb::core::logical_planner::plan
