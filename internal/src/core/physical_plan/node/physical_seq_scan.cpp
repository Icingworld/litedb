#include "core/physical_plan/node/physical_seq_scan.hpp"

#include <memory>
#include <utility>

namespace litedb::core::physical_plan
{

PhysicalSeqScan::PhysicalSeqScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : PhysicalPlanNode(PhysicalPlanNodeKind::SeqScan, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
{
}

common::DatabaseId PhysicalSeqScan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId PhysicalSeqScan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & PhysicalSeqScan::collection_name() const noexcept
{
    return collection_name_;
}

std::unique_ptr<PhysicalPlanNode> PhysicalSeqScan::clone() const
{
    return std::make_unique<PhysicalSeqScan>(database_id_, collection_id_, collection_name_, location());
}

} // namespace litedb::core::physical_plan
