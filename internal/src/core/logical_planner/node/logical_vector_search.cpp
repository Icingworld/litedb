#include "core/logical_planner/node/logical_vector_search.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalVectorSearch::LogicalVectorSearch(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::VIndexId index_id,
    std::string index_name,
    common::ColumnId column_id,
    std::string column_name,
    meta::entry::VectorDistanceMetric metric,
    std::unique_ptr<binder::bound::BoundExpression> query_vector,
    std::unique_ptr<binder::bound::BoundExpression> predicate,
    std::size_t required_count,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::VectorSearch, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , index_id_(index_id)
    , index_name_(std::move(index_name))
    , column_id_(column_id)
    , column_name_(std::move(column_name))
    , metric_(metric)
    , query_vector_(std::move(query_vector))
    , predicate_(std::move(predicate))
    , required_count_(required_count)
{
}

common::DatabaseId LogicalVectorSearch::database_id() const noexcept { return database_id_; }
common::CollectionId LogicalVectorSearch::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalVectorSearch::collection_name() const noexcept { return collection_name_; }
common::VIndexId LogicalVectorSearch::index_id() const noexcept { return index_id_; }
const std::string & LogicalVectorSearch::index_name() const noexcept { return index_name_; }
common::ColumnId LogicalVectorSearch::column_id() const noexcept { return column_id_; }
const std::string & LogicalVectorSearch::column_name() const noexcept { return column_name_; }
meta::entry::VectorDistanceMetric LogicalVectorSearch::metric() const noexcept { return metric_; }
const binder::bound::BoundExpression & LogicalVectorSearch::query_vector() const noexcept { return *query_vector_; }
const binder::bound::BoundExpression * LogicalVectorSearch::predicate() const noexcept { return predicate_.get(); }
std::size_t LogicalVectorSearch::required_count() const noexcept { return required_count_; }

void LogicalVectorSearch::accept(LogicalPlanNodeVisitor & visitor) const { visitor.visit(*this); }

std::unique_ptr<LogicalPlanNode> LogicalVectorSearch::clone() const
{
    return std::make_unique<LogicalVectorSearch>(
        database_id_, collection_id_, collection_name_, index_id_, index_name_, column_id_, column_name_, metric_,
        query_vector_->clone(), predicate_ ? predicate_->clone() : nullptr, required_count_, location()
    );
}

} // namespace litedb::core::planner::logical
