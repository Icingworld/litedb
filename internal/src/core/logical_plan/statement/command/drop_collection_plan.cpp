#include "core/logical_plan/statement/command/drop_collection_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

DropCollectionPlan::DropCollectionPlan(
    common::DatabaseId database_id,
    std::optional<common::CollectionId> collection_id,
    std::string collection_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::DropCollection, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , if_exists_(if_exists)
{
}

common::DatabaseId DropCollectionPlan::database_id() const noexcept
{
    return database_id_;
}

std::optional<common::CollectionId> DropCollectionPlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & DropCollectionPlan::collection_name() const noexcept
{
    return collection_name_;
}

bool DropCollectionPlan::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::planner::plan
