#include "core/logical_planner/plan/command/create_collection_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

CreateCollectionPlan::CreateCollectionPlan(
    common::DatabaseId database_id,
    std::optional<std::string> collection_name,
    std::vector<catalog::ColumnDefinition> columns,
    std::optional<std::string> comment
)
    : LogicalPlan(LogicalPlanKind::CreateCollection)
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

std::optional<std::string> CreateCollectionPlan::take_collection_name() noexcept
{
    return std::exchange(collection_name_, std::nullopt);
}

const std::vector<catalog::ColumnDefinition> & CreateCollectionPlan::columns() const noexcept
{
    return columns_;
}

std::vector<catalog::ColumnDefinition> CreateCollectionPlan::take_columns() noexcept
{
    return std::exchange(columns_, {});
}

std::optional<const std::string &> CreateCollectionPlan::comment() const noexcept
{
    return comment_;
}

std::optional<std::string> CreateCollectionPlan::take_comment() noexcept
{
    return std::exchange(comment_, std::nullopt);
}

} // namespace litedb::core::logical_planner::plan
