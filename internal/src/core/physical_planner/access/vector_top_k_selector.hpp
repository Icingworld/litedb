#pragma once

#include <cstddef>
#include <optional>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/catalog/entry/vector_index_entry.hpp"
#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::physical_planner
{

// 向量 TopK 访问路径选择结果
struct VectorTopKDecision
{
    common::CollectionId collection_id {0};
    common::VIndexId index_id {0};
    common::ColumnId column_id {0};
    catalog::entry::VectorDistanceMetric metric;
    common::Value query_value;
    common::LogicalType query_type;
    std::size_t query_argument_index {0};
    std::size_t required_count {0};
    bool has_filter {false};
};

// 向量 TopK 访问路径选择器
class VectorTopKSelector final
{
public:
    explicit VectorTopKSelector(const PhysicalPlannerContext & context) noexcept;

public:
    // 选择向量 TopK 访问路径
    // 无法安全降级为向量索引时返回 nullopt
    [[nodiscard]]
    std::optional<VectorTopKDecision> select(
        const logical_planner::op::LogicalLimitOperator & limit
    ) const;

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
