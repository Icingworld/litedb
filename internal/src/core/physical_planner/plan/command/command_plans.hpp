#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include <utility>

#include "core/common/ids.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/meta/meta_request.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

class UsePlan final : public PhysicalPlan
{
public:
    explicit UsePlan(common::DatabaseId database_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::Use)
        , database_id_(database_id)
    {
    }

    [[nodiscard]] common::DatabaseId database_id() const noexcept { return database_id_; }

private:
    common::DatabaseId database_id_;
};

class CreateDatabasePlan final : public PhysicalPlan
{
public:
    explicit CreateDatabasePlan(std::optional<std::string> database_name)
        : PhysicalPlan(PhysicalPlanKind::CreateDatabase)
        , database_name_(std::move(database_name))
    {
    }

    [[nodiscard]] const std::optional<std::string> & database_name() const noexcept
    {
        return database_name_;
    }

private:
    std::optional<std::string> database_name_;
};

class CreateCollectionPlan final : public PhysicalPlan
{
public:
    CreateCollectionPlan(
        common::DatabaseId database_id,
        std::optional<std::string> collection_name,
        std::vector<meta::ColumnDefinition> columns,
        std::optional<std::string> comment
    )
        : PhysicalPlan(PhysicalPlanKind::CreateCollection)
        , database_id_(database_id)
        , collection_name_(std::move(collection_name))
        , columns_(std::move(columns))
        , comment_(std::move(comment))
    {
    }

    [[nodiscard]] common::DatabaseId database_id() const noexcept { return database_id_; }
    [[nodiscard]] const std::optional<std::string> & collection_name() const noexcept { return collection_name_; }
    [[nodiscard]] const std::vector<meta::ColumnDefinition> & columns() const noexcept { return columns_; }
    [[nodiscard]] const std::optional<std::string> & comment() const noexcept { return comment_; }

private:
    common::DatabaseId database_id_;
    std::optional<std::string> collection_name_;
    std::vector<meta::ColumnDefinition> columns_;
    std::optional<std::string> comment_;
};

class CreateIndexPlan final : public PhysicalPlan
{
public:
    CreateIndexPlan(
        common::ColumnId column_id,
        std::optional<std::string> index_name,
        meta::entry::IndexKind index_kind,
        bool unique
    )
        : PhysicalPlan(PhysicalPlanKind::CreateIndex)
        , column_id_(column_id)
        , index_name_(std::move(index_name))
        , index_kind_(index_kind)
        , unique_(unique)
    {
    }

    [[nodiscard]] common::ColumnId column_id() const noexcept { return column_id_; }
    [[nodiscard]] const std::optional<std::string> & index_name() const noexcept { return index_name_; }
    [[nodiscard]] meta::entry::IndexKind index_kind() const noexcept { return index_kind_; }
    [[nodiscard]] bool unique() const noexcept { return unique_; }

private:
    common::ColumnId column_id_;
    std::optional<std::string> index_name_;
    meta::entry::IndexKind index_kind_;
    bool unique_;
};

class CreateVectorIndexPlan final : public PhysicalPlan
{
public:
    CreateVectorIndexPlan(
        common::ColumnId column_id,
        std::optional<std::string> index_name,
        meta::entry::VectorIndexKind index_kind,
        meta::entry::VectorDistanceMetric metric,
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
    {
    }

    [[nodiscard]] common::ColumnId column_id() const noexcept { return column_id_; }
    [[nodiscard]] const std::optional<std::string> & index_name() const noexcept { return index_name_; }
    [[nodiscard]] meta::entry::VectorIndexKind index_kind() const noexcept { return index_kind_; }
    [[nodiscard]] meta::entry::VectorDistanceMetric metric() const noexcept { return metric_; }
    [[nodiscard]] std::size_t max_neighbors() const noexcept { return max_neighbors_; }
    [[nodiscard]] std::size_t ef_construction() const noexcept { return ef_construction_; }
    [[nodiscard]] std::size_t ef_search_default() const noexcept { return ef_search_default_; }
    [[nodiscard]] std::size_t random_seed() const noexcept { return random_seed_; }

private:
    common::ColumnId column_id_;
    std::optional<std::string> index_name_;
    meta::entry::VectorIndexKind index_kind_;
    meta::entry::VectorDistanceMetric metric_;
    std::size_t max_neighbors_;
    std::size_t ef_construction_;
    std::size_t ef_search_default_;
    std::size_t random_seed_;
};

class DropDatabasePlan final : public PhysicalPlan
{
public:
    explicit DropDatabasePlan(std::optional<common::DatabaseId> database_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::DropDatabase)
        , database_id_(database_id)
    {
    }

    [[nodiscard]] std::optional<common::DatabaseId> database_id() const noexcept { return database_id_; }

private:
    std::optional<common::DatabaseId> database_id_;
};

class DropCollectionPlan final : public PhysicalPlan
{
public:
    explicit DropCollectionPlan(std::optional<common::CollectionId> collection_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::DropCollection)
        , collection_id_(collection_id)
    {
    }

    [[nodiscard]] std::optional<common::CollectionId> collection_id() const noexcept { return collection_id_; }

private:
    std::optional<common::CollectionId> collection_id_;
};

class DropIndexPlan final : public PhysicalPlan
{
public:
    explicit DropIndexPlan(std::optional<common::IndexId> index_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::DropIndex)
        , index_id_(index_id)
    {
    }

    [[nodiscard]] std::optional<common::IndexId> index_id() const noexcept { return index_id_; }

private:
    std::optional<common::IndexId> index_id_;
};

class DropVectorIndexPlan final : public PhysicalPlan
{
public:
    explicit DropVectorIndexPlan(std::optional<common::VIndexId> index_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::DropVectorIndex)
        , index_id_(index_id)
    {
    }

    [[nodiscard]] std::optional<common::VIndexId> index_id() const noexcept { return index_id_; }

private:
    std::optional<common::VIndexId> index_id_;
};

class ShowDatabasesPlan final : public PhysicalPlan
{
public:
    ShowDatabasesPlan() noexcept
        : PhysicalPlan(PhysicalPlanKind::ShowDatabases)
    {
    }
};

class ShowCollectionsPlan final : public PhysicalPlan
{
public:
    explicit ShowCollectionsPlan(common::DatabaseId database_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::ShowCollections)
        , database_id_(database_id)
    {
    }

    [[nodiscard]] common::DatabaseId database_id() const noexcept { return database_id_; }

private:
    common::DatabaseId database_id_;
};

class ShowIndexesPlan final : public PhysicalPlan
{
public:
    explicit ShowIndexesPlan(common::CollectionId collection_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::ShowIndexes)
        , collection_id_(collection_id)
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept { return collection_id_; }

private:
    common::CollectionId collection_id_;
};

class ShowVectorIndexesPlan final : public PhysicalPlan
{
public:
    explicit ShowVectorIndexesPlan(common::CollectionId collection_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::ShowVectorIndexes)
        , collection_id_(collection_id)
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept { return collection_id_; }

private:
    common::CollectionId collection_id_;
};

class DescribeCollectionPlan final : public PhysicalPlan
{
public:
    explicit DescribeCollectionPlan(common::CollectionId collection_id) noexcept
        : PhysicalPlan(PhysicalPlanKind::DescribeCollection)
        , collection_id_(collection_id)
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept { return collection_id_; }

private:
    common::CollectionId collection_id_;
};

} // namespace litedb::core::physical_planner::plan
