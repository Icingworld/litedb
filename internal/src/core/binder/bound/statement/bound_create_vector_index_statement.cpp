#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateVectorIndexStatement::BoundCreateVectorIndexStatement(
    common::ColumnId column_id,
    std::optional<std::string> vector_index_name,
    meta::entry::VectorIndexKind vector_index_kind,
    meta::entry::VectorDistanceMetric metric,
    std::size_t max_neighbors,
    std::size_t ef_construction,
    std::size_t ef_search_default,
    std::size_t random_seed
)
    : BoundStatement(BoundStatementKind::CreateVectorIndex)
    , column_id_(column_id)
    , vector_index_name_(std::move(vector_index_name))
    , vector_index_kind_(vector_index_kind)
    , metric_(metric)
    , max_neighbors_(max_neighbors)
    , ef_construction_(ef_construction)
    , ef_search_default_(ef_search_default)
    , random_seed_(random_seed)
{}

std::optional<const std::string &>
BoundCreateVectorIndexStatement::vector_index_name() const noexcept
{
    return vector_index_name_;
}

std::optional<std::string> BoundCreateVectorIndexStatement::take_vector_index_name() noexcept
{
    return std::exchange(vector_index_name_, std::nullopt);
}

common::ColumnId BoundCreateVectorIndexStatement::column_id() const noexcept
{
    return column_id_;
}

meta::entry::VectorIndexKind BoundCreateVectorIndexStatement::vector_index_kind() const noexcept
{
    return vector_index_kind_;
}

meta::entry::VectorDistanceMetric BoundCreateVectorIndexStatement::metric() const noexcept
{
    return metric_;
}

std::size_t BoundCreateVectorIndexStatement::max_neighbors() const noexcept
{
    return max_neighbors_;
}

std::size_t BoundCreateVectorIndexStatement::ef_construction() const noexcept
{
    return ef_construction_;
}

std::size_t BoundCreateVectorIndexStatement::ef_search_default() const noexcept
{
    return ef_search_default_;
}

std::size_t BoundCreateVectorIndexStatement::random_seed() const noexcept
{
    return random_seed_;
}

} // namespace litedb::core::binder::bound
