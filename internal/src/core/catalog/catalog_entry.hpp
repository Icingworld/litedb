#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/catalog/catalog_default_expression.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_id.hpp"

namespace litedb::core::catalog
{

enum class CatalogEntryKind : std::uint8_t
{
    Database,
    Collection,
    Column,
    Index,
    VectorIndex,
};

[[nodiscard]]
std::string normalize_identifier(std::string_view name);

class CatalogEntry
{
public:
    CatalogEntry(CatalogEntryKind kind, std::uint64_t id, std::string name);

    virtual ~CatalogEntry() noexcept = default;

    CatalogEntry(const CatalogEntry &) = delete;

    CatalogEntry & operator=(const CatalogEntry &) = delete;

    CatalogEntry(CatalogEntry &&) noexcept = default;

    CatalogEntry & operator=(CatalogEntry &&) noexcept = default;

    [[nodiscard]]
    CatalogEntryKind kind() const noexcept;

    [[nodiscard]]
    std::uint64_t raw_id() const noexcept;

    [[nodiscard]]
    const std::string & name() const noexcept;

    [[nodiscard]]
    const std::string & key() const noexcept;

private:
    CatalogEntryKind kind_;
    std::uint64_t id_;
    std::string name_;
    std::string key_;
};

class DatabaseEntry final : public CatalogEntry
{
public:
    DatabaseEntry(common::DatabaseId id, std::string name);

    [[nodiscard]]
    common::DatabaseId id() const noexcept;

    [[nodiscard]]
    const std::vector<common::CollectionId> & collection_ids() const noexcept;

    [[nodiscard]]
    std::optional<common::CollectionId> find_collection_id(std::string_view collection_key) const;

    void add_collection(std::string_view collection_key, common::CollectionId collection_id);

    void remove_collection(std::string_view collection_key, common::CollectionId collection_id);

private:
    std::vector<common::CollectionId> collection_ids_;
    std::unordered_map<std::string, common::CollectionId> collections_by_key_;
};

class CollectionEntry final : public CatalogEntry
{
public:
    CollectionEntry(common::CollectionId id, common::DatabaseId database_id, std::string name);

    [[nodiscard]]
    common::CollectionId id() const noexcept;

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    const std::vector<common::ColumnId> & column_ids() const noexcept;

    [[nodiscard]]
    std::optional<common::ColumnId> primary_key_column_id() const noexcept;

    [[nodiscard]]
    std::optional<common::ColumnId> find_column_id(std::string_view column_key) const;

    void add_column(std::string_view column_key, common::ColumnId column_id, bool primary_key);

private:
    common::DatabaseId database_id_;
    std::vector<common::ColumnId> column_ids_;
    std::unordered_map<std::string, common::ColumnId> columns_by_key_;
    std::optional<common::ColumnId> primary_key_column_id_;
};

class ColumnEntry final : public CatalogEntry
{
public:
    ColumnEntry(
        common::ColumnId id,
        common::CollectionId collection_id,
        std::string name,
        common::LogicalType type,
        bool primary_key,
        bool unique,
        bool nullable,
        std::optional<CatalogDefaultExpression> default_expression,
        std::optional<std::string> comment
    );

    [[nodiscard]]
    common::ColumnId id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    [[nodiscard]]
    bool primary_key() const noexcept;

    [[nodiscard]]
    bool unique() const noexcept;

    [[nodiscard]]
    bool nullable() const noexcept;

    [[nodiscard]]
    const std::optional<CatalogDefaultExpression> & default_expression() const noexcept;

    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::CollectionId collection_id_;
    common::LogicalType type_;
    bool primary_key_;
    bool unique_;
    bool nullable_;
    std::optional<CatalogDefaultExpression> default_expression_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::catalog
