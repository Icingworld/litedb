#pragma once

#include <cstddef>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/catalog/catalog_entry.hpp"

namespace litedb::core::binder::bound
{

class BoundCreateVectorIndexStatement final : public BoundStatement
{
public:
    BoundCreateVectorIndexStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        std::string index_name,
        catalog::CatalogVectorIndexKind index_kind,
        catalog::CatalogVectorDistanceMetric metric,
        std::size_t max_neighbors,
        std::size_t ef_construction,
        std::size_t ef_search_default,
        std::size_t random_seed,
        bool if_not_exists,
        parser::ast::AstNodeLocation location
    );

public:
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    [[nodiscard]]
    const std::string & column_name() const noexcept;

    [[nodiscard]]
    const std::string & index_name() const noexcept;

    [[nodiscard]]
    catalog::CatalogVectorIndexKind index_kind() const noexcept;

    [[nodiscard]]
    catalog::CatalogVectorDistanceMetric metric() const noexcept;

    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    [[nodiscard]]
    std::size_t ef_search_default() const noexcept;

    [[nodiscard]]
    std::size_t random_seed() const noexcept;

    [[nodiscard]]
    bool if_not_exists() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    common::ColumnId column_id_;
    std::string column_name_;
    std::string index_name_;
    catalog::CatalogVectorIndexKind index_kind_;
    catalog::CatalogVectorDistanceMetric metric_;
    std::size_t max_neighbors_;
    std::size_t ef_construction_;
    std::size_t ef_search_default_;
    std::size_t random_seed_;
    bool if_not_exists_;
};

} // namespace litedb::core::binder::bound
