#pragma once

#include <optional>
#include <utility>

#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

enum class IndexLookupKind
{
    Equal,
    Range,
};

struct IndexBound
{
    index::ScalarIndexKey key;
    bool inclusive {true};
};

struct IndexLookup
{
    IndexLookupKind kind {IndexLookupKind::Equal};
    std::optional<IndexBound> lower;
    std::optional<IndexBound> upper;
};

class IndexScanOperator final : public PhysicalOperator
{
public:
    IndexScanOperator(
        common::CollectionId collection_id,
        common::IndexId index_id,
        IndexLookup lookup
    ) noexcept
        : PhysicalOperator(PhysicalOperatorKind::IndexScan)
        , collection_id_(collection_id)
        , index_id_(index_id)
        , lookup_(std::move(lookup))
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept
    {
        return collection_id_;
    }

    [[nodiscard]] common::IndexId index_id() const noexcept
    {
        return index_id_;
    }

    [[nodiscard]] const IndexLookup & lookup() const noexcept
    {
        return lookup_;
    }

private:
    common::CollectionId collection_id_;
    common::IndexId index_id_;
    IndexLookup lookup_;
};

} // namespace litedb::core::physical_planner::op
