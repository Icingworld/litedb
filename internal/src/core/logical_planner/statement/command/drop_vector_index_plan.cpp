#include "core/logical_planner/statement/command/drop_vector_index_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

DropVectorIndexPlan::DropVectorIndexPlan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::string index_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : LogicalStatementPlan(LogicalStatementPlanKind::DropVectorIndex, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , index_name_(std::move(index_name))
    , if_exists_(if_exists)
{
}

common::DatabaseId DropVectorIndexPlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId DropVectorIndexPlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & DropVectorIndexPlan::collection_name() const noexcept
{
    return collection_name_;
}

const std::string & DropVectorIndexPlan::index_name() const noexcept
{
    return index_name_;
}

bool DropVectorIndexPlan::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::planner::plan
