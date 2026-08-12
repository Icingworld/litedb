#include "core/physical_planner/plan/command/create_vector_index_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

CreateVectorIndexPlan::CreateVectorIndexPlan(
    common::ColumnId column_id,
    std::optional<std::string> index_name,
    catalog::entry::VectorIndexKind index_kind,
    catalog::entry::VectorDistanceMetric metric,
    std::size_t max_neighbors,
    std::size_t ef_construction,
    std::size_t ef_search_default,
    std::size_t random_seed
)
    : PhysicalPlan(PhysicalPlanKind::CreateVectorIndex)
    , column_id_(column_id)
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , metric_(metric)
    , max_neighbors_(max_neighbors)
    , ef_construction_(ef_construction)
    , ef_search_default_(ef_search_default)
    , random_seed_(random_seed)
{}

common::ColumnId CreateVectorIndexPlan::column_id() const noexcept
{
    return column_id_;
}

std::optional<const std::string &> CreateVectorIndexPlan::index_name() const noexcept
{
    return index_name_;
}

catalog::entry::VectorIndexKind CreateVectorIndexPlan::index_kind() const noexcept
{
    return index_kind_;
}

catalog::entry::VectorDistanceMetric CreateVectorIndexPlan::metric() const noexcept
{
    return metric_;
}

std::size_t CreateVectorIndexPlan::max_neighbors() const noexcept
{
    return max_neighbors_;
}

std::size_t CreateVectorIndexPlan::ef_construction() const noexcept
{
    return ef_construction_;
}

std::size_t CreateVectorIndexPlan::ef_search_default() const noexcept
{
    return ef_search_default_;
}

std::size_t CreateVectorIndexPlan::random_seed() const noexcept
{
    return random_seed_;
}

} // namespace litedb::core::physical_planner::plan