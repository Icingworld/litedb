#include "core/physical_plan/statement/physical_row_mutation_plan.hpp"

#include <utility>

namespace litedb::core::physical_plan
{

PhysicalDeletePlan::PhysicalDeletePlan(
    std::unique_ptr<PhysicalPlanNode> input,
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : PhysicalStatementPlan(PhysicalStatementPlanKind::Delete, location)
    , input_(std::move(input))
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
{
}

const PhysicalPlanNode & PhysicalDeletePlan::input() const noexcept
{
    return *input_;
}

common::DatabaseId PhysicalDeletePlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId PhysicalDeletePlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & PhysicalDeletePlan::collection_name() const noexcept
{
    return collection_name_;
}

PhysicalUpdatePlan::PhysicalUpdatePlan(
    std::unique_ptr<PhysicalPlanNode> input,
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<binder::bound::BoundAssignment> assignments,
    parser::ast::AstNodeLocation location
)
    : PhysicalStatementPlan(PhysicalStatementPlanKind::Update, location)
    , input_(std::move(input))
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , assignments_(std::move(assignments))
{
}

const PhysicalPlanNode & PhysicalUpdatePlan::input() const noexcept
{
    return *input_;
}

common::DatabaseId PhysicalUpdatePlan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId PhysicalUpdatePlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & PhysicalUpdatePlan::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<binder::bound::BoundAssignment> & PhysicalUpdatePlan::assignments() const noexcept
{
    return assignments_;
}

} // namespace litedb::core::physical_plan
