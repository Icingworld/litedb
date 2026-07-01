#include "core/planner/plan/mutation/insert_plan.hpp"

#include <utility>

namespace litedb::core::planner::plan
{

InsertPlan::InsertPlan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<binder::bound::BoundColumn> columns,
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::Insert, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , columns_(std::move(columns))
    , values_(std::move(values))
{
}

common::DatabaseId InsertPlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId InsertPlan::collection_id() const noexcept {
    return collection_id_;
}

const std::string & InsertPlan::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<binder::bound::BoundColumn> & InsertPlan::columns() const noexcept
{
    return columns_;
}

const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & InsertPlan::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::planner::plan
