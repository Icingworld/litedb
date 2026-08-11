#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_projection_item.hpp"
#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 投影算子
class ProjectionOperator final : public PhysicalUnaryOperator
{
public:
    ProjectionOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::vector<binder::bound::BoundProjectionItem> projections
    ) noexcept;

public:
    // 获取投影项
    [[nodiscard]]
    const std::vector<binder::bound::BoundProjectionItem> & projections() const noexcept;

private:
    std::vector<binder::bound::BoundProjectionItem> projections_;
};

} // namespace litedb::core::physical_planner::op
