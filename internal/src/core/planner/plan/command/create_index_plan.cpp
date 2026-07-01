#include "core/planner/plan/command/create_index_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

CreateIndexPlan::CreateIndexPlan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::ColumnId column_id,
    std::string column_name,
    std::string index_name,
    catalog::CatalogIndexKind index_kind,
    bool unique,
    bool if_not_exists,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::CreateIndex, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , column_id_(column_id)
    , column_name_(std::move(column_name))
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , unique_(unique)
    , if_not_exists_(if_not_exists)
{
}

common::DatabaseId CreateIndexPlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId CreateIndexPlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & CreateIndexPlan::collection_name() const noexcept
{
    return collection_name_;
}

common::ColumnId CreateIndexPlan::column_id() const noexcept
{
    return column_id_;
}

const std::string & CreateIndexPlan::column_name() const noexcept
{
    return column_name_;
}

const std::string & CreateIndexPlan::index_name() const noexcept
{
    return index_name_;
}

catalog::CatalogIndexKind CreateIndexPlan::index_kind() const noexcept
{
    return index_kind_;
}

bool CreateIndexPlan::unique() const noexcept
{
    return unique_;
}

bool CreateIndexPlan::if_not_exists() const noexcept
{
    return if_not_exists_;
}

} // namespace litedb::core::planner::plan
