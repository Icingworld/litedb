#pragma once

#include <cstddef>
#include <optional>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::physical_planner
{

/**
 * @brief 向量 TopK 访问路径选择结果
 * @details 只包含后续 lowering 所需的值，不暴露逻辑树中的裸指针。
 */
struct VectorTopKDecision
{
    common::CollectionId collection_id {0};
    common::VIndexId index_id {0};
    common::ColumnId column_id {0};
    meta::entry::VectorDistanceMetric metric;
    common::Value query_value;
    common::LogicalType query_type;
    std::size_t query_argument_index {0};
    std::size_t required_count {0};
    bool has_filter {false};
};

/**
 * @brief 选择可由 HNSW 执行的向量 TopK 访问路径
 */
class VectorTopKSelector final
{
public:
    explicit VectorTopKSelector(const PhysicalPlannerContext & context) noexcept
        : context_(context)
    {
    }

public:
    /**
     * @brief 选择向量 TopK 访问路径
     * @param limit 逻辑 LIMIT 算子
     * @return 选择结果；无法安全降级为向量索引时返回 nullopt
     */
    [[nodiscard]]
    std::optional<VectorTopKDecision> select(
        const logical_planner::op::LogicalLimitOperator & limit
    ) const;

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
