#include "core/physical_planner/physical_planner.hpp"

#include <cassert>
#include <limits>
#include <optional>
#include <utility>
#include <variant>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/common/identifier.hpp"
#include "core/common/types.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/command/create_collection_plan.hpp"
#include "core/logical_planner/plan/command/create_database_plan.hpp"
#include "core/logical_planner/plan/command/create_index_plan.hpp"
#include "core/logical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/describe_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_database_plan.hpp"
#include "core/logical_planner/plan/command/drop_index_plan.hpp"
#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/show_collections_plan.hpp"
#include "core/logical_planner/plan/command/show_databases_plan.hpp"
#include "core/logical_planner/plan/command/show_indexes_plan.hpp"
#include "core/logical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/logical_planner/plan/command/use_plan.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"
#include "core/physical_planner/plan/command/create_collection_plan.hpp"
#include "core/physical_planner/plan/command/create_database_plan.hpp"
#include "core/physical_planner/plan/command/create_index_plan.hpp"
#include "core/physical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/describe_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_database_plan.hpp"
#include "core/physical_planner/plan/command/drop_index_plan.hpp"
#include "core/physical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/show_collections_plan.hpp"
#include "core/physical_planner/plan/command/show_databases_plan.hpp"
#include "core/physical_planner/plan/command/show_indexes_plan.hpp"
#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace litedb::core::physical_planner
{

namespace
{

using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundColumnRefExpression;
using binder::bound::BoundExpression;
using common::BinaryOperator;

/**
 * @brief 向量 TopK 匹配结果
 */
struct VectorTopKMatch
{
    const logical_planner::op::LogicalLimitOperator * limit {nullptr};           ///< LIMIT 算子
    const logical_planner::op::LogicalOrderByOperator * order_by {nullptr};      ///< ORDER BY 算子
    const logical_planner::op::LogicalProjectionOperator * projection {nullptr}; ///< 投影算子
    const logical_planner::op::LogicalFilterOperator * filter {nullptr};         ///< 可选过滤算子
    const logical_planner::op::LogicalScanOperator * scan {nullptr};             ///< 扫描算子
    const binder::bound::BoundFunctionExpression * distance {nullptr};           ///< 距离函数
    std::size_t query_argument_index {0};                                        ///< 查询向量参数下标
    common::ColumnId column_id;                                                  ///< 向量列 ID
    meta::entry::VectorDistanceMetric metric;                                    ///< 距离度量
};

/**
 * @brief 标量索引候选
 */
struct IndexCandidate
{
    common::ColumnId column_id;     ///< 列 ID
    op::IndexLookup lookup;         ///< 查找条件
};

/**
 * @brief 求值常量表达式
 * @param expression 表达式
 * @return 常量值，失败则为 nullopt
 */
[[nodiscard]]
std::optional<common::Value> evaluate_constant(
    const BoundExpression & expression
)
{
    auto value = evaluator::ExpressionEvaluator::evaluate_constant(expression);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return std::move(*value);
}

/**
 * @brief 由比较运算构造索引查找条件
 * @param operation 比较运算符
 * @param value_expression 常量值表达式
 * @return 查找条件，无法构造则为 nullopt
 */
[[nodiscard]]
std::optional<op::IndexLookup> make_lookup(
    BinaryOperator operation,
    const BoundExpression & value_expression
)
{
    auto value = evaluate_constant(value_expression);
    if (!value.has_value()) {
        return std::nullopt;
    }

    auto key = index::ScalarIndexKey::from_value(std::move(*value));
    if (!key.has_value()) {
        return std::nullopt;
    }

    switch (operation) {
    case BinaryOperator::Equal:
        return op::IndexLookup {
            .kind = op::IndexLookupKind::Equal,
            .lower = op::IndexBound {
                .key = std::move(*key),
                .inclusive = true,
            },
        };
    case BinaryOperator::GreaterThan:
        return op::IndexLookup {
            .kind = op::IndexLookupKind::Range,
            .lower = op::IndexBound {
                .key = std::move(*key),
                .inclusive = false,
            },
        };
    case BinaryOperator::GreaterThanOrEqual:
        return op::IndexLookup {
            .kind = op::IndexLookupKind::Range,
            .lower = op::IndexBound {
                .key = std::move(*key),
                .inclusive = true,
            },
        };
    case BinaryOperator::LessThan:
        return op::IndexLookup {
            .kind = op::IndexLookupKind::Range,
            .upper = op::IndexBound {
                .key = std::move(*key),
                .inclusive = false,
            },
        };
    case BinaryOperator::LessThanOrEqual:
        return op::IndexLookup {
            .kind = op::IndexLookupKind::Range,
            .upper = op::IndexBound {
                .key = std::move(*key),
                .inclusive = true,
            },
        };
    default:
        return std::nullopt;
    }
}

/**
 * @brief 反转比较运算符
 * @param operation 比较运算符
 * @return 反转后的比较运算符
 */
[[nodiscard]]
BinaryOperator reverse_comparison(BinaryOperator operation) noexcept
{
    switch (operation) {
    case BinaryOperator::LessThan:
        return BinaryOperator::GreaterThan;
    case BinaryOperator::LessThanOrEqual:
        return BinaryOperator::GreaterThanOrEqual;
    case BinaryOperator::GreaterThan:
        return BinaryOperator::LessThan;
    case BinaryOperator::GreaterThanOrEqual:
        return BinaryOperator::LessThanOrEqual;
    default:
        return operation;
    }
}

/**
 * @brief 从谓词提取标量索引候选
 * @param predicate 谓词
 * @return 索引候选，无法提取则为 nullopt
 */
[[nodiscard]]
std::optional<IndexCandidate> candidate_from_predicate(
    const BoundExpression & predicate
)
{
    if (predicate.kind() == binder::bound::BoundExpressionKind::Binary) {
        const auto & binary = static_cast<const BoundBinaryExpression &>(predicate);
        const BoundColumnRefExpression * column = nullptr;
        const BoundExpression * value = nullptr;
        auto operation = binary.op();

        if (binary.left().kind() == binder::bound::BoundExpressionKind::ColumnRef) {
            column = &static_cast<const BoundColumnRefExpression &>(binary.left());
            value = &binary.right();
        } else if (binary.right().kind() == binder::bound::BoundExpressionKind::ColumnRef) {
            column = &static_cast<const BoundColumnRefExpression &>(binary.right());
            value = &binary.left();
            operation = reverse_comparison(operation);
        }

        if (column == nullptr || value == nullptr) {
            return std::nullopt;
        }

        auto lookup = make_lookup(operation, *value);
        if (!lookup.has_value()) {
            return std::nullopt;
        }

        return IndexCandidate {
            .column_id = column->column_id(),
            .lookup = std::move(*lookup),
        };
    }

    if (predicate.kind() == binder::bound::BoundExpressionKind::Between) {
        const auto & between = static_cast<const BoundBetweenExpression &>(predicate);
        if (between.expression().kind() != binder::bound::BoundExpressionKind::ColumnRef) {
            return std::nullopt;
        }

        const auto & column = static_cast<const BoundColumnRefExpression &>(
            between.expression()
        );
        auto lower_value = evaluate_constant(between.lower());
        auto upper_value = evaluate_constant(between.upper());
        if (!lower_value.has_value() || !upper_value.has_value()) {
            return std::nullopt;
        }

        auto lower = index::ScalarIndexKey::from_value(std::move(*lower_value));
        auto upper = index::ScalarIndexKey::from_value(std::move(*upper_value));
        if (!lower.has_value() || !upper.has_value()) {
            return std::nullopt;
        }

        return IndexCandidate {
            .column_id = column.column_id(),
            .lookup = op::IndexLookup {
                .kind = op::IndexLookupKind::Range,
                .lower = op::IndexBound {
                    .key = std::move(*lower),
                    .inclusive = true,
                },
                .upper = op::IndexBound {
                    .key = std::move(*upper),
                    .inclusive = true,
                },
            },
        };
    }

    return std::nullopt;
}

/**
 * @brief 为候选选择标量索引
 * @param catalog 目录视图
 * @param collection_id 集合 ID
 * @param candidate 索引候选
 * @return 选中的索引 ID，无可用索引则为 nullopt
 */
[[nodiscard]]
std::optional<common::IndexId> choose_index(
    const meta::CatalogView & catalog,
    common::CollectionId collection_id,
    const IndexCandidate & candidate
)
{
    std::optional<common::IndexId> selected;
    for (const auto * entry : catalog.list_indexes(collection_id)) {
        if (entry == nullptr
            || entry->collection_id() != collection_id
            || entry->column_id() != candidate.column_id
            || entry->kind() != meta::entry::IndexKind::BTree) {
            continue;
        }
        if (candidate.lookup.kind == op::IndexLookupKind::Range
            && entry->kind() != meta::entry::IndexKind::BTree) {
            continue;
        }
        if (!selected.has_value() || entry->id() < *selected) {
            selected = entry->id();
        }
    }
    return selected;
}

/**
 * @brief 由距离函数与排序方向推导向量度量
 * @param distance 距离函数表达式
 * @param ascending 是否升序
 * @return 向量度量，不匹配则为 nullopt
 */
[[nodiscard]]
std::optional<meta::entry::VectorDistanceMetric> vector_metric(
    const binder::bound::BoundFunctionExpression & distance,
    bool ascending
)
{
    const auto name = common::normalize_identifier(distance.function().name());
    if (name == "l2_distance" && ascending) {
        return meta::entry::VectorDistanceMetric::L2;
    }
    if (name == "cosine_distance" && ascending) {
        return meta::entry::VectorDistanceMetric::Cosine;
    }
    if (name == "inner_product" && !ascending) {
        return meta::entry::VectorDistanceMetric::InnerProduct;
    }
    return std::nullopt;
}

/**
 * @brief 判断表达式是否为常量向量
 * @param expression 表达式
 * @return 是否为常量向量
 */
[[nodiscard]]
bool is_constant_vector(const BoundExpression & expression)
{
    auto value = evaluate_constant(expression);
    return value.has_value()
        && std::holds_alternative<common::VectorValue>(value->data());
}

/**
 * @brief 匹配可降级为向量检索的 TopK 模式
 * @param limit LIMIT 算子
 * @param catalog 目录视图
 * @return 匹配结果，不匹配则为 nullopt
 */
[[nodiscard]]
std::optional<VectorTopKMatch> match_vector_top_k(
    const logical_planner::op::LogicalLimitOperator & limit,
    const meta::CatalogView & catalog
)
{
    if (!limit.limit().has_value()
        || limit.limit().value() == 0
        || limit.child().kind() != logical_planner::op::LogicalPlanOperatorKind::OrderBy) {
        return std::nullopt;
    }

    const auto & order_by = static_cast<const logical_planner::op::LogicalOrderByOperator &>(
        limit.child()
    );
    if (order_by.order_by().size() != 1
        || order_by.child().kind() != logical_planner::op::LogicalPlanOperatorKind::Projection) {
        return std::nullopt;
    }

    const auto & projection = static_cast<const logical_planner::op::LogicalProjectionOperator &>(
        order_by.child()
    );

    const logical_planner::op::LogicalFilterOperator * filter = nullptr;
    const logical_planner::op::LogicalScanOperator * scan = nullptr;
    if (projection.child().kind() == logical_planner::op::LogicalPlanOperatorKind::Scan) {
        scan = &static_cast<const logical_planner::op::LogicalScanOperator &>(
            projection.child()
        );
    } else if (projection.child().kind() == logical_planner::op::LogicalPlanOperatorKind::Filter) {
        filter = &static_cast<const logical_planner::op::LogicalFilterOperator &>(
            projection.child()
        );
        if (filter->child().kind() != logical_planner::op::LogicalPlanOperatorKind::Scan) {
            return std::nullopt;
        }
        scan = &static_cast<const logical_planner::op::LogicalScanOperator &>(
            filter->child()
        );
    } else {
        return std::nullopt;
    }

    const auto & item = order_by.order_by().front();
    if (item.expression == nullptr
        || item.expression->kind() != binder::bound::BoundExpressionKind::Function) {
        return std::nullopt;
    }

    const auto & distance = static_cast<const binder::bound::BoundFunctionExpression &>(
        *item.expression
    );
    const auto metric = vector_metric(distance, item.ascending);
    if (!metric.has_value() || distance.arguments().size() != 2) {
        return std::nullopt;
    }

    std::size_t query_argument_index = 0;
    common::ColumnId column_id {};
    bool matched = false;
    for (std::size_t index = 0; index < distance.arguments().size(); ++index) {
        const auto other = index == 0 ? 1U : 0U;
        if (distance.arguments()[index] == nullptr
            || distance.arguments()[other] == nullptr
            || distance.arguments()[index]->kind() != binder::bound::BoundExpressionKind::ColumnRef
            || !is_constant_vector(*distance.arguments()[other])) {
            continue;
        }

        const auto & column = static_cast<const binder::bound::BoundColumnRefExpression &>(
            *distance.arguments()[index]
        );
        const auto * entry = catalog.find_column(column.column_id());
        if (entry == nullptr
            || entry->collection_id() != scan->collection_id()
            || entry->type().id != common::LogicalTypeId::Vector) {
            continue;
        }

        query_argument_index = other;
        column_id = column.column_id();
        matched = true;
        break;
    }
    if (!matched) {
        return std::nullopt;
    }

    return VectorTopKMatch {
        .limit = &limit,
        .order_by = &order_by,
        .projection = &projection,
        .filter = filter,
        .scan = scan,
        .distance = &distance,
        .query_argument_index = query_argument_index,
        .column_id = column_id,
        .metric = *metric,
    };
}

/**
 * @brief 为向量 TopK 匹配选择向量索引
 * @param catalog 目录视图
 * @param match 匹配结果
 * @return 选中的向量索引 ID，无可用索引则为 nullopt
 */
[[nodiscard]]
std::optional<common::VIndexId> choose_vector_index(
    const meta::CatalogView & catalog,
    const VectorTopKMatch & match
)
{
    std::optional<common::VIndexId> selected;
    for (const auto * entry : catalog.list_vector_indexes(match.scan->collection_id())) {
        if (entry == nullptr
            || entry->column_id() != match.column_id
            || entry->index_kind() != meta::entry::VectorIndexKind::Hnsw
            || entry->metric() != match.metric) {
            continue;
        }
        if (!selected.has_value() || entry->id() < *selected) {
            selected = entry->id();
        }
    }
    return selected;
}

} // namespace

PhysicalPlanner::PhysicalPlanner(meta::CatalogView catalog) noexcept
    : catalog_(catalog)
{
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::plan(
    std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
)
{
    assert(logical_plan != nullptr);
    return dispatch_plan(*logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_use_plan(
    logical_planner::plan::UsePlan & logical_plan
)
{
    return std::make_unique<plan::UsePlan>(logical_plan.database_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_create_database_plan(
    logical_planner::plan::CreateDatabasePlan & logical_plan
)
{
    return std::make_unique<plan::CreateDatabasePlan>(logical_plan.database_name());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_create_collection_plan(
    logical_planner::plan::CreateCollectionPlan & logical_plan
)
{
    return std::make_unique<plan::CreateCollectionPlan>(
        logical_plan.database_id(),
        logical_plan.collection_name(),
        logical_plan.columns(),
        logical_plan.comment()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_create_index_plan(
    logical_planner::plan::CreateIndexPlan & logical_plan
)
{
    return std::make_unique<plan::CreateIndexPlan>(
        logical_plan.column_id(),
        logical_plan.index_name(),
        logical_plan.index_kind(),
        logical_plan.unique()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_create_vector_index_plan(
    logical_planner::plan::CreateVectorIndexPlan & logical_plan
)
{
    return std::make_unique<plan::CreateVectorIndexPlan>(
        logical_plan.column_id(),
        logical_plan.vector_index_name(),
        logical_plan.vector_index_kind(),
        logical_plan.metric(),
        logical_plan.max_neighbors(),
        logical_plan.ef_construction(),
        logical_plan.ef_search_default(),
        logical_plan.random_seed()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_drop_database_plan(
    logical_planner::plan::DropDatabasePlan & logical_plan
)
{
    return std::make_unique<plan::DropDatabasePlan>(logical_plan.database_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_drop_collection_plan(
    logical_planner::plan::DropCollectionPlan & logical_plan
)
{
    return std::make_unique<plan::DropCollectionPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_drop_index_plan(
    logical_planner::plan::DropIndexPlan & logical_plan
)
{
    return std::make_unique<plan::DropIndexPlan>(logical_plan.index_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_drop_vector_index_plan(
    logical_planner::plan::DropVectorIndexPlan & logical_plan
)
{
    return std::make_unique<plan::DropVectorIndexPlan>(logical_plan.vector_index_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_show_databases_plan(
    logical_planner::plan::ShowDatabasesPlan &
)
{
    return std::make_unique<plan::ShowDatabasesPlan>();
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_show_collections_plan(
    logical_planner::plan::ShowCollectionsPlan & logical_plan
)
{
    return std::make_unique<plan::ShowCollectionsPlan>(logical_plan.database_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_show_indexes_plan(
    logical_planner::plan::ShowIndexesPlan & logical_plan
)
{
    return std::make_unique<plan::ShowIndexesPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_show_vector_indexes_plan(
    logical_planner::plan::ShowVectorIndexesPlan & logical_plan
)
{
    return std::make_unique<plan::ShowVectorIndexesPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_describe_collection_plan(
    logical_planner::plan::DescribeCollectionPlan & logical_plan
)
{
    return std::make_unique<plan::DescribeCollectionPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_insert_plan(
    logical_planner::plan::InsertPlan & logical_plan
)
{
    return std::make_unique<plan::InsertPlan>(
        logical_plan.collection_id(),
        logical_plan.take_values()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_update_plan(
    logical_planner::plan::UpdatePlan & logical_plan
)
{
    auto collection_id = logical_plan.collection_id();
    auto assignments = logical_plan.take_assignments();
    auto root_operator = lower_operator(logical_plan.take_root_operator());
    return std::make_unique<plan::UpdatePlan>(
        collection_id,
        std::move(assignments),
        std::move(root_operator)
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_delete_plan(
    logical_planner::plan::DeletePlan & logical_plan
)
{
    auto collection_id = logical_plan.collection_id();
    auto root_operator = lower_operator(logical_plan.take_root_operator());
    return std::make_unique<plan::DeletePlan>(
        collection_id,
        std::move(root_operator)
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::visit_query_plan(
    logical_planner::plan::QueryPlan & logical_plan
)
{
    return std::make_unique<plan::QueryPlan>(
        lower_operator(logical_plan.take_root_operator())
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalPlanner::lower_operator(
    std::unique_ptr<logical_planner::op::LogicalPlanOperator> logical_operator
)
{
    assert(logical_operator != nullptr);
    return dispatch_operator(*logical_operator.release());
}

std::unique_ptr<op::PhysicalOperator> PhysicalPlanner::visit_scan_operator(
    logical_planner::op::LogicalScanOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalScanOperator> owned(&logical_operator);
    return std::make_unique<op::SeqScanOperator>(owned->collection_id());
}

std::unique_ptr<op::PhysicalOperator> PhysicalPlanner::visit_filter_operator(
    logical_planner::op::LogicalFilterOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalFilterOperator> owned(&logical_operator);
    const auto & logical_child = owned->child();

    if (logical_child.kind() == logical_planner::op::LogicalPlanOperatorKind::Scan) {
        const auto & scan = static_cast<const logical_planner::op::LogicalScanOperator &>(
            logical_child
        );
        const auto candidate = candidate_from_predicate(owned->predicate());
        if (candidate.has_value()) {
            const auto selected = choose_index(
                catalog_,
                scan.collection_id(),
                *candidate
            );
            if (selected.has_value()) {
                auto predicate = owned->take_predicate();
                auto physical_scan = std::make_unique<op::IndexScanOperator>(
                    scan.collection_id(),
                    *selected,
                    std::move(candidate->lookup)
                );
                return std::make_unique<op::FilterOperator>(
                    std::move(physical_scan),
                    std::move(predicate)
                );
            }
        }
    }

    auto child = lower_operator(owned->take_child());
    auto predicate = owned->take_predicate();
    return std::make_unique<op::FilterOperator>(
        std::move(child),
        std::move(predicate)
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalPlanner::visit_projection_operator(
    logical_planner::op::LogicalProjectionOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalProjectionOperator> owned(&logical_operator);
    auto child = lower_operator(owned->take_child());
    return std::make_unique<op::ProjectionOperator>(
        std::move(child),
        owned->take_projections()
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalPlanner::visit_order_by_operator(
    logical_planner::op::LogicalOrderByOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalOrderByOperator> owned(&logical_operator);
    auto child = lower_operator(owned->take_child());
    return std::make_unique<op::SortOperator>(
        std::move(child),
        owned->take_order_by()
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalPlanner::visit_limit_operator(
    logical_planner::op::LogicalLimitOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalLimitOperator> owned(&logical_operator);

    const auto match = match_vector_top_k(*owned, catalog_);
    if (match.has_value()) {
        const auto offset = owned->offset().value_or(0);
        if (offset <= std::numeric_limits<std::size_t>::max() - owned->limit().value()) {
            const auto index_id = choose_vector_index(catalog_, *match);
            if (index_id.has_value()) {
                auto query_value = evaluate_constant(
                    *match->distance->arguments()[match->query_argument_index]
                );
                if (!query_value.has_value()) {
                    auto child = lower_operator(owned->take_child());
                    return std::make_unique<op::LimitOperator>(
                        std::move(child),
                        owned->limit(),
                        owned->offset()
                    );
                }

                auto order = std::unique_ptr<logical_planner::op::LogicalOrderByOperator>(
                    static_cast<logical_planner::op::LogicalOrderByOperator *>(
                        owned->take_child().release()
                    )
                );
                auto projection = std::unique_ptr<logical_planner::op::LogicalProjectionOperator>(
                    static_cast<logical_planner::op::LogicalProjectionOperator *>(
                        order->take_child().release()
                    )
                );

                auto base = projection->take_child();
                std::unique_ptr<binder::bound::BoundExpression> predicate;
                if (match->filter != nullptr) {
                    auto filter = std::unique_ptr<logical_planner::op::LogicalFilterOperator>(
                        static_cast<logical_planner::op::LogicalFilterOperator *>(
                            base.release()
                        )
                    );
                    predicate = filter->take_predicate();
                    base = filter->take_child();
                }

                auto scan = std::unique_ptr<logical_planner::op::LogicalScanOperator>(
                    static_cast<logical_planner::op::LogicalScanOperator *>(
                        base.release()
                    )
                );
                const auto collection_id = scan->collection_id();

                auto order_items = order->take_order_by();
                auto & distance = static_cast<binder::bound::BoundFunctionExpression &>(
                    *order_items.front().expression
                );
                const auto query_type = distance.arguments()[match->query_argument_index]->type();
                auto query_vector = std::make_unique<binder::bound::BoundLiteralExpression>(
                    query_type,
                    std::move(*query_value)
                );

                auto projection_items = projection->take_projections();
                auto search = std::make_unique<op::VectorSearchOperator>(
                    collection_id,
                    *index_id,
                    match->column_id,
                    match->metric,
                    std::move(query_vector),
                    std::move(predicate),
                    owned->limit().value() + offset
                );
                auto projected = std::make_unique<op::ProjectionOperator>(
                    std::move(search),
                    std::move(projection_items)
                );
                auto sorted = std::make_unique<op::SortOperator>(
                    std::move(projected),
                    std::move(order_items)
                );
                return std::make_unique<op::LimitOperator>(
                    std::move(sorted),
                    owned->limit(),
                    owned->offset()
                );
            }
        }
    }

    auto child = lower_operator(owned->take_child());
    return std::make_unique<op::LimitOperator>(
        std::move(child),
        owned->limit(),
        owned->offset()
    );
}

} // namespace litedb::core::physical_planner
