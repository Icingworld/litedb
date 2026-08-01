#include "core/logical_planner/statement/mutation/update_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

UpdatePlan::UpdatePlan(
    std::unique_ptr<logical::LogicalPlanNode> input,
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<binder::bound::BoundAssignment> assignments,
    parser::ast::AstNodeLocation location
)
    : LogicalStatementPlan(LogicalStatementPlanKind::Update, location)
    , input_(std::move(input))
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , assignments_(std::move(assignments))
{
}

const logical::LogicalPlanNode & UpdatePlan::input() const noexcept
{
    return *input_;
}

common::DatabaseId UpdatePlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId UpdatePlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & UpdatePlan::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<binder::bound::BoundAssignment> & UpdatePlan::assignments() const noexcept
{
    return assignments_;
}

} // namespace litedb::core::planner::plan
