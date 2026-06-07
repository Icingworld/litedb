#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/catalog/catalog.hpp"
#include "core/catalog/catalog_entry.hpp"

namespace litedb::core::catalog
{

class InMemoryCatalog final : public Catalog
{
public:
    InMemoryCatalog() = default;

    [[nodiscard]]
    const DatabaseEntry * find_database(std::string_view name) const override;

    [[nodiscard]]
    const DatabaseEntry * find_database(common::DatabaseId database_id) const override;

    [[nodiscard]]
    const CollectionEntry * find_collection(
        common::DatabaseId database_id,
        std::string_view name
    ) const override;

    [[nodiscard]]
    const CollectionEntry * find_collection(common::CollectionId collection_id) const override;

    [[nodiscard]]
    const ColumnEntry * find_column(common::CollectionId collection_id, std::string_view name) const override;

    [[nodiscard]]
    const ColumnEntry * find_column(common::ColumnId column_id) const override;

    [[nodiscard]]
    std::vector<const DatabaseEntry *> list_databases() const override;

    [[nodiscard]]
    std::vector<const CollectionEntry *> list_collections(common::DatabaseId database_id) const override;

    [[nodiscard]]
    std::vector<const ColumnEntry *> list_columns(common::CollectionId collection_id) const override;

    std::expected<common::DatabaseId, CatalogError> create_database(
        const CreateDatabaseRequest & request
    ) override;

    std::expected<void, CatalogError> drop_database(const DropDatabaseRequest & request) override;

    std::expected<common::CollectionId, CatalogError> create_collection(
        const CreateCollectionRequest & request
    ) override;

    std::expected<void, CatalogError> drop_collection(const DropCollectionRequest & request) override;

private:
    [[nodiscard]]
    DatabaseEntry * find_database_mutable(common::DatabaseId database_id);

    [[nodiscard]]
    std::expected<void, CatalogError> validate_collection_request(const CreateCollectionRequest & request) const;

    [[nodiscard]]
    common::DatabaseId next_database_id() noexcept;

    [[nodiscard]]
    common::CollectionId next_collection_id() noexcept;

    [[nodiscard]]
    common::ColumnId next_column_id() noexcept;

private:
    common::DatabaseId next_database_id_ {1};
    common::CollectionId next_collection_id_ {1};
    common::ColumnId next_column_id_ {1};

    std::vector<common::DatabaseId> database_ids_;
    std::unordered_map<common::DatabaseId, std::unique_ptr<DatabaseEntry>> databases_by_id_;
    std::unordered_map<std::string, common::DatabaseId> databases_by_key_;

    std::unordered_map<common::CollectionId, std::unique_ptr<CollectionEntry>> collections_by_id_;
    std::unordered_map<common::ColumnId, std::unique_ptr<ColumnEntry>> columns_by_id_;
};

} // namespace litedb::core::catalog
