#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/meta/meta.hpp"
#include "core/meta/meta.hpp"
#include "core/common/ids.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

class PhysicalUsePlan final : public PhysicalStatementPlan
{
public:
    PhysicalUsePlan(
        common::DatabaseId database_id,
        std::string database_name,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::Use, location)
        , database_id_(database_id)
        , database_name_(std::move(database_name))
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    const std::string & database_name() const noexcept { return database_name_; }

private:
    common::DatabaseId database_id_;
    std::string database_name_;
};

class PhysicalCreateDatabasePlan final : public PhysicalStatementPlan
{
public:
    PhysicalCreateDatabasePlan(
        std::string database_name,
        bool if_not_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::CreateDatabase, location)
        , database_name_(std::move(database_name))
        , if_not_exists_(if_not_exists)
    {
    }

    [[nodiscard]]
    const std::string & database_name() const noexcept { return database_name_; }

    [[nodiscard]]
    bool if_not_exists() const noexcept { return if_not_exists_; }

private:
    std::string database_name_;
    bool if_not_exists_ {false};
};

class PhysicalCreateCollectionPlan final : public PhysicalStatementPlan
{
public:
    PhysicalCreateCollectionPlan(
        common::DatabaseId database_id,
        std::string collection_name,
        bool if_not_exists,
        std::vector<meta::ColumnDefinition> columns,
        std::optional<std::string> comment,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::CreateCollection, location)
        , database_id_(database_id)
        , collection_name_(std::move(collection_name))
        , if_not_exists_(if_not_exists)
        , columns_(std::move(columns))
        , comment_(std::move(comment))
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    bool if_not_exists() const noexcept { return if_not_exists_; }

    [[nodiscard]]
    const std::vector<meta::ColumnDefinition> & columns() const noexcept { return columns_; }

    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept { return comment_; }

private:
    common::DatabaseId database_id_;
    std::string collection_name_;
    bool if_not_exists_ {false};
    std::vector<meta::ColumnDefinition> columns_;
    std::optional<std::string> comment_;
};

class PhysicalCreateIndexPlan final : public PhysicalStatementPlan
{
public:
    PhysicalCreateIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        std::string index_name,
        meta::entry::IndexKind index_kind,
        bool unique,
        bool if_not_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::CreateIndex, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
        , column_id_(column_id)
        , column_name_(std::move(column_name))
        , index_name_(std::move(index_name))
        , index_kind_(index_kind)
        , unique_(unique)
        , if_not_exists_(if_not_exists)
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    common::ColumnId column_id() const noexcept { return column_id_; }

    [[nodiscard]]
    const std::string & column_name() const noexcept { return column_name_; }

    [[nodiscard]]
    const std::string & index_name() const noexcept { return index_name_; }

    [[nodiscard]]
    meta::entry::IndexKind index_kind() const noexcept { return index_kind_; }

    [[nodiscard]]
    bool unique() const noexcept { return unique_; }

    [[nodiscard]]
    bool if_not_exists() const noexcept { return if_not_exists_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    common::ColumnId column_id_;
    std::string column_name_;
    std::string index_name_;
    meta::entry::IndexKind index_kind_;
    bool unique_ {false};
    bool if_not_exists_ {false};
};

class PhysicalCreateVectorIndexPlan final : public PhysicalStatementPlan
{
public:
    PhysicalCreateVectorIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        std::string index_name,
        meta::entry::VectorIndexKind index_kind,
        meta::entry::VectorDistanceMetric metric,
        std::size_t max_neighbors,
        std::size_t ef_construction,
        std::size_t ef_search_default,
        std::size_t random_seed,
        bool if_not_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::CreateVectorIndex, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
        , column_id_(column_id)
        , column_name_(std::move(column_name))
        , index_name_(std::move(index_name))
        , index_kind_(index_kind)
        , metric_(metric)
        , max_neighbors_(max_neighbors)
        , ef_construction_(ef_construction)
        , ef_search_default_(ef_search_default)
        , random_seed_(random_seed)
        , if_not_exists_(if_not_exists)
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    common::ColumnId column_id() const noexcept { return column_id_; }

    [[nodiscard]]
    const std::string & column_name() const noexcept { return column_name_; }

    [[nodiscard]]
    const std::string & index_name() const noexcept { return index_name_; }

    [[nodiscard]]
    meta::entry::VectorIndexKind index_kind() const noexcept { return index_kind_; }

    [[nodiscard]]
    meta::entry::VectorDistanceMetric metric() const noexcept { return metric_; }

    [[nodiscard]]
    std::size_t max_neighbors() const noexcept { return max_neighbors_; }

    [[nodiscard]]
    std::size_t ef_construction() const noexcept { return ef_construction_; }

    [[nodiscard]]
    std::size_t ef_search_default() const noexcept { return ef_search_default_; }

    [[nodiscard]]
    std::size_t random_seed() const noexcept { return random_seed_; }

    [[nodiscard]]
    bool if_not_exists() const noexcept { return if_not_exists_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    common::ColumnId column_id_;
    std::string column_name_;
    std::string index_name_;
    meta::entry::VectorIndexKind index_kind_;
    meta::entry::VectorDistanceMetric metric_;
    std::size_t max_neighbors_ {0};
    std::size_t ef_construction_ {0};
    std::size_t ef_search_default_ {0};
    std::size_t random_seed_ {0};
    bool if_not_exists_ {false};
};

class PhysicalDropDatabasePlan final : public PhysicalStatementPlan
{
public:
    PhysicalDropDatabasePlan(
        std::optional<common::DatabaseId> database_id,
        std::string database_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::DropDatabase, location)
        , database_id_(database_id)
        , database_name_(std::move(database_name))
        , if_exists_(if_exists)
    {
    }

    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    const std::string & database_name() const noexcept { return database_name_; }

    [[nodiscard]]
    bool if_exists() const noexcept { return if_exists_; }

private:
    std::optional<common::DatabaseId> database_id_;
    std::string database_name_;
    bool if_exists_ {false};
};

class PhysicalDropCollectionPlan final : public PhysicalStatementPlan
{
public:
    PhysicalDropCollectionPlan(
        common::DatabaseId database_id,
        std::optional<common::CollectionId> collection_id,
        std::string collection_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::DropCollection, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
        , if_exists_(if_exists)
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    std::optional<common::CollectionId> collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    bool if_exists() const noexcept { return if_exists_; }

private:
    common::DatabaseId database_id_;
    std::optional<common::CollectionId> collection_id_;
    std::string collection_name_;
    bool if_exists_ {false};
};

class PhysicalDropIndexPlan final : public PhysicalStatementPlan
{
public:
    PhysicalDropIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::string index_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::DropIndex, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
        , index_name_(std::move(index_name))
        , if_exists_(if_exists)
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    const std::string & index_name() const noexcept { return index_name_; }

    [[nodiscard]]
    bool if_exists() const noexcept { return if_exists_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::string index_name_;
    bool if_exists_ {false};
};

class PhysicalDropVectorIndexPlan final : public PhysicalStatementPlan
{
public:
    PhysicalDropVectorIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::string index_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::DropVectorIndex, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
        , index_name_(std::move(index_name))
        , if_exists_(if_exists)
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    const std::string & index_name() const noexcept { return index_name_; }

    [[nodiscard]]
    bool if_exists() const noexcept { return if_exists_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::string index_name_;
    bool if_exists_ {false};
};

class PhysicalShowDatabasesPlan final : public PhysicalStatementPlan
{
public:
    explicit PhysicalShowDatabasesPlan(parser::ast::AstNodeLocation location)
        : PhysicalStatementPlan(PhysicalStatementPlanKind::ShowDatabases, location)
    {
    }
};

class PhysicalShowCollectionsPlan final : public PhysicalStatementPlan
{
public:
    PhysicalShowCollectionsPlan(common::DatabaseId database_id, parser::ast::AstNodeLocation location)
        : PhysicalStatementPlan(PhysicalStatementPlanKind::ShowCollections, location)
        , database_id_(database_id)
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

private:
    common::DatabaseId database_id_;
};

class PhysicalShowIndexesPlan final : public PhysicalStatementPlan
{
public:
    PhysicalShowIndexesPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::ShowIndexes, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

class PhysicalShowVectorIndexesPlan final : public PhysicalStatementPlan
{
public:
    PhysicalShowVectorIndexesPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::ShowVectorIndexes, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

class PhysicalDescribeCollectionPlan final : public PhysicalStatementPlan
{
public:
    PhysicalDescribeCollectionPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::DescribeCollection, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

} // namespace litedb::core::physical_plan
