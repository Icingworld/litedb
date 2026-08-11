#pragma once

#include <optional>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::physical_planner
{

// 标量索引访问路径选择结果
struct ScalarAccessPath
{
    common::IndexId index_id {0};
    op::IndexLookup lookup;
};

// 标量索引访问路径选择器
class ScalarAccessPathSelector final
{
public:
    explicit ScalarAccessPathSelector(const PhysicalPlannerContext & context) noexcept;

public:
    // 选择标量索引访问路径
    // 无法使用索引时返回 nullopt
    [[nodiscard]]
    std::optional<ScalarAccessPath> select(
        common::CollectionId collection_id,
        const binder::bound::BoundExpression & predicate
    ) const;

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
