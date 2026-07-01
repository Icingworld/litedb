#include "core/planner/plan/command/describe_collection_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

DescribeCollectionPlan::DescribeCollectionPlan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::DescribeCollection, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
{
}

common::DatabaseId DescribeCollectionPlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId DescribeCollectionPlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & DescribeCollectionPlan::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::planner::plan
