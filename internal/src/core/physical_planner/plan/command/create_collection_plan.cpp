#include "core/physical_planner/plan/command/create_collection_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

CreateCollectionPlan::CreateCollectionPlan(
    common::DatabaseId database_id,
    std::optional<std::string> collection_name,
    std::vector<catalog::ColumnDefinition> columns,
    std::optional<std::string> comment
)
    : PhysicalPlan(PhysicalPlanKind::CreateCollection)
    , database_id_(database_id)
    , collection_name_(std::move(collection_name))
    , columns_(std::move(columns))
    , comment_(std::move(comment))
{}

common::DatabaseId CreateCollectionPlan::database_id() const noexcept
{
    return database_id_;
}

std::optional<const std::string &> CreateCollectionPlan::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<catalog::ColumnDefinition> & CreateCollectionPlan::columns() const noexcept
{
    return columns_;
}

const std::optional<std::string> & CreateCollectionPlan::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::physical_planner::plan