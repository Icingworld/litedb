#include "core/planner/access_path/access_path_selector.hpp"

#include <utility>

#include "core/planner/logical/node/logical_scan.hpp"

namespace litedb::core::planner::access_path
{

namespace
{

using binder::bound::BoundExpression;

[[nodiscard]]
std::unique_ptr<logical::LogicalPlanNode> make_scan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
{
    return std::make_unique<logical::LogicalScan>(
        database_id,
        collection_id,
        std::move(collection_name),
        location
    );
}

} // namespace

AccessPathSelector::AccessPathSelector(const index::IndexManager * index_manager) noexcept
    : index_manager_(index_manager)
{
}

std::unique_ptr<logical::LogicalPlanNode> AccessPathSelector::select_scan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    const BoundExpression * predicate,
    parser::ast::AstNodeLocation location
) const
{
    (void) predicate;
    return make_scan(database_id, collection_id, std::move(collection_name), location);
}

} // namespace litedb::core::planner::access_path
