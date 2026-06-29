#include "core/planner/statement/create_collection_plan.hpp"

#include <utility>

namespace litedb::core::planner
{

CreateCollectionPlan::CreateCollectionPlan(
    common::DatabaseId database_id,
    std::string collection_name,
    bool if_not_exists,
    std::vector<catalog::ColumnDefinition> columns,
    std::optional<std::string> comment,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::CreateCollection, location)
    , database_id_(database_id)
    , collection_name_(std::move(collection_name))
    , if_not_exists_(if_not_exists)
    , columns_(std::move(columns))
    , comment_(std::move(comment))
{
}

common::DatabaseId CreateCollectionPlan::database_id() const noexcept
{
    return database_id_;
}

const std::string & CreateCollectionPlan::collection_name() const noexcept
{
    return collection_name_;
}

bool CreateCollectionPlan::if_not_exists() const noexcept
{
    return if_not_exists_;
}

const std::vector<catalog::ColumnDefinition> & CreateCollectionPlan::columns() const noexcept
{
    return columns_;
}

const std::optional<std::string> & CreateCollectionPlan::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::planner
