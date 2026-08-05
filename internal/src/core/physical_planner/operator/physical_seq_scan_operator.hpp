#pragma once

#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

class SeqScanOperator final : public PhysicalOperator
{
public:
    explicit SeqScanOperator(common::CollectionId collection_id) noexcept
        : PhysicalOperator(PhysicalOperatorKind::SeqScan)
        , collection_id_(collection_id)
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept
    {
        return collection_id_;
    }

private:
    common::CollectionId collection_id_;
};

} // namespace litedb::core::physical_planner::op
