#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/meta/meta.hpp"
#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/physical_plan/node/physical_plan_node.hpp"

namespace litedb::core::physical_plan
{

enum class PhysicalIndexLookupKind
{
    Equal,
    Range,
};

struct PhysicalIndexBound
{
    index::ScalarIndexKey key;
    bool inclusive {true};
};

struct PhysicalIndexLookup
{
    PhysicalIndexLookupKind kind {PhysicalIndexLookupKind::Equal};
    std::optional<PhysicalIndexBound> lower;
    std::optional<PhysicalIndexBound> upper;
};

class PhysicalIndexScan final : public PhysicalPlanNode
{
public:
    PhysicalIndexScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::IndexId index_id,
        std::string index_name,
        meta::entry::IndexKind index_kind,
        common::ColumnId column_id,
        std::string column_name,
        PhysicalIndexLookup lookup,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    [[nodiscard]]
    const std::string & index_name() const noexcept;

    [[nodiscard]]
    meta::entry::IndexKind index_kind() const noexcept;

    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    [[nodiscard]]
    const std::string & column_name() const noexcept;

    [[nodiscard]]
    const PhysicalIndexLookup & lookup() const noexcept;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> clone() const override;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    common::IndexId index_id_;
    std::string index_name_;
    meta::entry::IndexKind index_kind_;
    common::ColumnId column_id_;
    std::string column_name_;
    PhysicalIndexLookup lookup_;
};

} // namespace litedb::core::physical_plan
