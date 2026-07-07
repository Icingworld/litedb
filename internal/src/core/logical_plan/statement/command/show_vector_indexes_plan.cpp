#include "core/logical_plan/statement/command/show_vector_indexes_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

ShowVectorIndexesPlan::ShowVectorIndexesPlan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::ShowVectorIndexes, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
{
}

common::DatabaseId ShowVectorIndexesPlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId ShowVectorIndexesPlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & ShowVectorIndexesPlan::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::planner::plan
