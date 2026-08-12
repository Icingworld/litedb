#include "core/physical_planner/access/vector_top_k_selector.hpp"

#include <limits>
#include <optional>
#include <utility>
#include <variant>

#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/common/identifier.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"

namespace litedb::core::physical_planner
{

namespace
{

using binder::bound::BoundExpression;
using binder::bound::BoundFunctionExpression;

// 评估常量表达式
[[nodiscard]]
std::optional<common::Value> evaluate_constant(const BoundExpression & expression)
{
    auto value = evaluator::ExpressionEvaluator::evaluate_constant(expression);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return std::move(*value);
}

// 转换向量距离度量
[[nodiscard]]
std::optional<catalog::entry::VectorDistanceMetric>
vector_metric(const BoundFunctionExpression & distance, bool ascending)
{
    const auto name = common::normalize_identifier(distance.function().name());
    if (name == "l2_distance" && ascending) {
        return catalog::entry::VectorDistanceMetric::L2;
    }
    if (name == "cosine_distance" && ascending) {
        return catalog::entry::VectorDistanceMetric::Cosine;
    }
    if (name == "inner_product" && !ascending) {
        return catalog::entry::VectorDistanceMetric::InnerProduct;
    }
    return std::nullopt;
}

// 选择向量索引
[[nodiscard]]
std::optional<common::VIndexId> choose_vector_index(
    const catalog::CatalogViewer & catalog,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    catalog::entry::VectorDistanceMetric metric
)
{
    std::optional<common::VIndexId> selected;
    for (const auto & entry_reference : catalog.list_vector_indexes(collection_id)) {
        const auto & entry = entry_reference.get();
        if (entry.column_id() != column_id ||
            entry.index_kind() != catalog::entry::VectorIndexKind::Hnsw ||
            entry.metric() != metric) {
            continue;
        }
        if (!selected.has_value() || entry.id() < *selected) {
            selected = entry.id();
        }
    }
    return selected;
}

} // namespace

VectorTopKSelector::VectorTopKSelector(const PhysicalPlannerContext & context) noexcept
    : context_(context)
{}

std::optional<VectorTopKDecision> VectorTopKSelector::select(
    const logical_planner::op::LogicalLimitOperator & limit
) const
{
    if (!limit.limit().has_value() || limit.limit().value() == 0 ||
        limit.child().kind() != logical_planner::op::LogicalPlanOperatorKind::OrderBy) {
        return std::nullopt;
    }

    const auto offset = limit.offset().value_or(0);
    if (offset > std::numeric_limits<std::size_t>::max() - limit.limit().value()) {
        return std::nullopt;
    }

    const auto & order_by =
        static_cast<const logical_planner::op::LogicalOrderByOperator &>(limit.child());
    if (order_by.order_by().size() != 1 ||
        order_by.child().kind() != logical_planner::op::LogicalPlanOperatorKind::Projection) {
        return std::nullopt;
    }

    const auto & projection =
        static_cast<const logical_planner::op::LogicalProjectionOperator &>(order_by.child());

    const logical_planner::op::LogicalFilterOperator * filter = nullptr;
    const logical_planner::op::LogicalScanOperator * scan = nullptr;
    if (projection.child().kind() == logical_planner::op::LogicalPlanOperatorKind::Scan) {
        scan = &static_cast<const logical_planner::op::LogicalScanOperator &>(projection.child());
    } else if (projection.child().kind() == logical_planner::op::LogicalPlanOperatorKind::Filter) {
        filter =
            &static_cast<const logical_planner::op::LogicalFilterOperator &>(projection.child());
        if (filter->child().kind() != logical_planner::op::LogicalPlanOperatorKind::Scan) {
            return std::nullopt;
        }
        scan = &static_cast<const logical_planner::op::LogicalScanOperator &>(filter->child());
    } else {
        return std::nullopt;
    }

    const auto & item = order_by.order_by().front();
    if (item.expression == nullptr ||
        item.expression->kind() != binder::bound::BoundExpressionKind::Function) {
        return std::nullopt;
    }

    const auto & distance = static_cast<const BoundFunctionExpression &>(*item.expression);
    const auto metric = vector_metric(distance, item.ascending);
    if (!metric.has_value() || distance.arguments().size() != 2) {
        return std::nullopt;
    }

    std::size_t query_argument_index = 0;
    common::ColumnId column_id {};
    common::LogicalType query_type {common::LogicalTypeId::Null, std::nullopt};
    std::optional<common::Value> query_value;
    bool matched = false;
    for (std::size_t index = 0; index < distance.arguments().size(); ++index) {
        const auto other = index == 0 ? 1U : 0U;
        if (distance.arguments()[index] == nullptr || distance.arguments()[other] == nullptr ||
            distance.arguments()[index]->kind() != binder::bound::BoundExpressionKind::ColumnRef) {
            continue;
        }

        auto candidate_value = evaluate_constant(*distance.arguments()[other]);
        if (!candidate_value.has_value() ||
            !std::holds_alternative<common::VectorValue>(candidate_value->data())) {
            continue;
        }

        const auto & column = static_cast<const binder::bound::BoundColumnRefExpression &>(
            *distance.arguments()[index]
        );
        const auto entry = context_.catalog().find_column(column.column_id());
        if (!entry || entry->collection_id() != scan->collection_id() ||
            entry->type().id != common::LogicalTypeId::Vector) {
            continue;
        }

        query_argument_index = other;
        column_id = column.column_id();
        query_type = distance.arguments()[other]->type();
        query_value = std::move(*candidate_value);
        matched = true;
        break;
    }
    if (!matched || !query_value.has_value()) {
        return std::nullopt;
    }

    const auto index_id =
        choose_vector_index(context_.catalog(), scan->collection_id(), column_id, *metric);
    if (!index_id.has_value()) {
        return std::nullopt;
    }

    return VectorTopKDecision {
        .collection_id = scan->collection_id(),
        .index_id = *index_id,
        .column_id = column_id,
        .metric = *metric,
        .query_value = std::move(*query_value),
        .query_type = query_type,
        .query_argument_index = query_argument_index,
        .required_count = limit.limit().value() + offset,
        .has_filter = filter != nullptr,
    };
}

} // namespace litedb::core::physical_planner
