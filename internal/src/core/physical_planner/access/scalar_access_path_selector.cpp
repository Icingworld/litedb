#include "core/physical_planner/access/scalar_access_path_selector.hpp"

#include <optional>
#include <utility>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/index/scalar_index_key.hpp"

namespace litedb::core::physical_planner
{

namespace
{

using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundColumnRefExpression;
using binder::bound::BoundExpression;
using common::BinaryOperator;

// 索引候选
struct IndexCandidate
{
    common::ColumnId column_id {0};
    op::IndexLookup lookup;
};

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

// 创建索引查找条件
[[nodiscard]]
std::optional<op::IndexLookup>
make_lookup(BinaryOperator operation, const BoundExpression & value_expression)
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

// 反转比较运算符
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

// 从谓词中选择索引候选
[[nodiscard]]
std::optional<IndexCandidate> candidate_from_predicate(const BoundExpression & predicate)
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

        const auto & column = static_cast<const BoundColumnRefExpression &>(between.expression());
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
                .lower =
                    op::IndexBound {
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

// 选择索引
[[nodiscard]]
std::optional<common::IndexId> choose_index(
    const meta::CatalogView & catalog,
    common::CollectionId collection_id,
    const IndexCandidate & candidate
)
{
    std::optional<common::IndexId> selected;
    for (const auto * entry : catalog.list_indexes(collection_id)) {
        if (entry == nullptr || entry->collection_id() != collection_id ||
            entry->column_id() != candidate.column_id ||
            entry->kind() != meta::entry::IndexKind::BTree) {
            continue;
        }
        if (candidate.lookup.kind == op::IndexLookupKind::Range &&
            entry->kind() != meta::entry::IndexKind::BTree) {
            continue;
        }
        if (!selected.has_value() || entry->id() < *selected) {
            selected = entry->id();
        }
    }
    return selected;
}

} // namespace

ScalarAccessPathSelector::ScalarAccessPathSelector(const PhysicalPlannerContext & context) noexcept
    : context_(context)
{}

std::optional<ScalarAccessPath> ScalarAccessPathSelector::select(
    common::CollectionId collection_id,
    const binder::bound::BoundExpression & predicate
) const
{
    const auto candidate = candidate_from_predicate(predicate);
    if (!candidate.has_value()) {
        return std::nullopt;
    }

    const auto index_id = choose_index(context_.catalog(), collection_id, *candidate);
    if (!index_id.has_value()) {
        return std::nullopt;
    }

    return ScalarAccessPath {
        .index_id = *index_id,
        .lookup = std::move(candidate->lookup),
    };
}

} // namespace litedb::core::physical_planner
