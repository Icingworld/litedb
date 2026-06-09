#include "core/planner/statement/delete_plan.hpp"

#include <utility>

namespace litedb::core::planner
{

DeletePlan::DeletePlan(
    std::unique_ptr<logical::LogicalPlanNode> input,
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::Delete, location)
    , input_(std::move(input))
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
{
}

const logical::LogicalPlanNode & DeletePlan::input() const noexcept
{
    return *input_;
}

common::DatabaseId DeletePlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId DeletePlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & DeletePlan::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::planner
