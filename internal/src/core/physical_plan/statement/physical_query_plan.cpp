#include "core/physical_plan/statement/physical_query_plan.hpp"

#include <utility>

namespace litedb::core::physical_plan
{

PhysicalQueryPlan::PhysicalQueryPlan(
    std::unique_ptr<PhysicalPlanNode> root,
    parser::ast::AstNodeLocation location
)
    : PhysicalStatementPlan(PhysicalStatementPlanKind::Query, location)
    , root_(std::move(root))
{
}

const PhysicalPlanNode & PhysicalQueryPlan::root() const noexcept
{
    return *root_;
}

} // namespace litedb::core::physical_plan
