#include "core/optimizer/optimizer.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/binder/bound/expression/bound_wildcard_expression.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/catalog/catalog_reader.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"
#include "core/logical_plan/statement/mutation/delete_plan.hpp"
#include "core/logical_plan/statement/mutation/update_plan.hpp"
#include "core/logical_plan/statement/query/query_plan.hpp"
#include "core/schema/record.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::optimizer
{

namespace
{

using binder::bound::BoundAssignment;
using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundCastExpression;
using binder::bound::BoundExpression;
using binder::bound::BoundExpressionKind;
using binder::bound::BoundFunctionExpression;
using binder::bound::BoundInExpression;
using binder::bound::BoundLikeExpression;
using binder::bound::BoundLiteralExpression;
using binder::bound::BoundNullExpression;
using binder::bound::BoundOrderByItem;
using binder::bound::BoundProjectionItem;
using binder::bound::BoundUnaryExpression;
using binder::bound::BoundVectorExpression;
using binder::bound::BoundWildcardExpression;
using planner::logical::LogicalFilter;
using planner::logical::LogicalIndexBound;
using planner::logical::LogicalIndexLookup;
using planner::logical::LogicalIndexLookupKind;
using planner::logical::LogicalLimit;
using planner::logical::LogicalOrderBy;
using planner::logical::LogicalPlanNode;
using planner::logical::LogicalPlanNodeKind;
using planner::logical::LogicalProjection;
using planner::logical::LogicalScan;
using planner::logical::LogicalScanIndexHint;
using planner::plan::DeletePlan;
using planner::plan::QueryPlan;
using planner::plan::LogicalStatementPlan;
using planner::plan::LogicalStatementPlanKind;
using planner::plan::UpdatePlan;
using parser::TokenType;

struct ExpressionRewriteResult
{
    std::unique_ptr<BoundExpression> expression;
    bool changed {false};
};

struct LogicalRewriteResult
{
    std::unique_ptr<LogicalPlanNode> node;
    bool changed {false};
};

struct IndexCandidate
{
    common::CollectionId collection_id;
    common::ColumnId column_id;
    LogicalIndexLookup lookup;
};

[[nodiscard]]
OptimizerError make_error(
    OptimizerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return OptimizerError {code, location, std::move(message)};
}

[[nodiscard]]
bool is_true_literal(const BoundExpression & expression)
{
    if (expression.kind() != BoundExpressionKind::Literal
        || expression.type().id != common::LogicalTypeId::Boolean) {
        return false;
    }
    const auto & literal = static_cast<const BoundLiteralExpression &>(expression);
    return literal.value() == "true" || literal.value() == "TRUE";
}

[[nodiscard]]
bool is_false_literal(const BoundExpression & expression)
{
    if (expression.kind() != BoundExpressionKind::Literal
        || expression.type().id != common::LogicalTypeId::Boolean) {
        return false;
    }
    const auto & literal = static_cast<const BoundLiteralExpression &>(expression);
    return literal.value() == "false" || literal.value() == "FALSE";
}

[[nodiscard]]
std::unique_ptr<BoundExpression> make_bool_literal(bool value, parser::ast::AstNodeLocation location)
{
    return std::make_unique<BoundLiteralExpression>(
        common::LogicalType {common::LogicalTypeId::Boolean, std::nullopt},
        value ? "true" : "false",
        location
    );
}

[[nodiscard]]
bool is_constant_foldable(const BoundExpression & expression)
{
    switch (expression.kind()) {
    case BoundExpressionKind::Literal:
    case BoundExpressionKind::Null:
        return true;
    case BoundExpressionKind::Unary: {
        const auto & unary = static_cast<const BoundUnaryExpression &>(expression);
        return is_constant_foldable(unary.operand());
    }
    case BoundExpressionKind::Binary: {
        const auto & binary = static_cast<const BoundBinaryExpression &>(expression);
        return is_constant_foldable(binary.left()) && is_constant_foldable(binary.right());
    }
    case BoundExpressionKind::Cast: {
        const auto & cast = static_cast<const BoundCastExpression &>(expression);
        return is_constant_foldable(cast.expression());
    }
    case BoundExpressionKind::ColumnRef:
    case BoundExpressionKind::Vector:
    case BoundExpressionKind::Function:
    case BoundExpressionKind::In:
    case BoundExpressionKind::Between:
    case BoundExpressionKind::Like:
    case BoundExpressionKind::Wildcard:
        return false;
    }

    return false;
}

[[nodiscard]]
std::string numeric_to_string(auto value)
{
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<decltype(value)>::max_digits10) << value;
    return out.str();
}

[[nodiscard]]
std::optional<std::unique_ptr<BoundExpression>> value_to_expression(
    const schema::Value & value,
    const BoundExpression & original
)
{
    if (value.is_null()) {
        return std::make_unique<BoundNullExpression>(original.type(), original.location());
    }

    return std::visit(
        [&original](const auto & data) -> std::optional<std::unique_ptr<BoundExpression>> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, schema::NullValue>) {
                return std::make_unique<BoundNullExpression>(original.type(), original.location());
            } else if constexpr (std::is_same_v<T, bool>) {
                return make_bool_literal(data, original.location());
            } else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t>) {
                return std::make_unique<BoundLiteralExpression>(original.type(), std::to_string(data), original.location());
            } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                return std::make_unique<BoundLiteralExpression>(original.type(), numeric_to_string(data), original.location());
            } else if constexpr (std::is_same_v<T, std::string>) {
                return std::make_unique<BoundLiteralExpression>(original.type(), data, original.location());
            } else {
                return std::nullopt;
            }
        },
        value.data()
    );
}

[[nodiscard]]
std::optional<std::unique_ptr<BoundExpression>> try_fold_constant(
    const BoundExpression & expression,
    const OptimizerOptions & options
)
{
    if (!options.enable_constant_folding || !is_constant_foldable(expression)) {
        return std::nullopt;
    }

    evaluator::ExpressionEvaluator evaluator;
    auto value = evaluator.evaluate(expression, schema::Record {});
    if (!value.has_value()) {
        return std::nullopt;
    }

    return value_to_expression(value.value(), expression);
}

[[nodiscard]]
std::optional<index::ScalarIndexKey> expression_to_index_key(const BoundExpression & expression)
{
    if (!is_constant_foldable(expression)) {
        return std::nullopt;
    }

    evaluator::ExpressionEvaluator evaluator;
    auto value = evaluator.evaluate(expression, schema::Record {});
    if (!value.has_value()) {
        return std::nullopt;
    }

    auto key = index::ScalarIndexKey::from_value(std::move(value.value()));
    if (!key.has_value()) {
        return std::nullopt;
    }
    return std::move(key.value());
}

[[nodiscard]]
const binder::bound::BoundColumnRefExpression * as_column_ref(const BoundExpression & expression)
{
    if (expression.kind() != BoundExpressionKind::ColumnRef) {
        return nullptr;
    }
    return &static_cast<const binder::bound::BoundColumnRefExpression &>(expression);
}

[[nodiscard]]
std::optional<TokenType> invert_comparison(TokenType op)
{
    switch (op) {
    case TokenType::LessThan:
        return TokenType::GreaterThan;
    case TokenType::LessEqual:
        return TokenType::GreaterEqual;
    case TokenType::GreaterThan:
        return TokenType::LessThan;
    case TokenType::GreaterEqual:
        return TokenType::LessEqual;
    case TokenType::Equal:
        return TokenType::Equal;
    default:
        return std::nullopt;
    }
}

[[nodiscard]]
std::optional<LogicalIndexLookup> lookup_from_comparison(TokenType op, index::ScalarIndexKey key)
{
    switch (op) {
    case TokenType::Equal:
        return LogicalIndexLookup {
            .kind = LogicalIndexLookupKind::Equal,
            .lower = LogicalIndexBound {.key = std::move(key), .inclusive = true},
        };
    case TokenType::GreaterThan:
        return LogicalIndexLookup {
            .kind = LogicalIndexLookupKind::Range,
            .lower = LogicalIndexBound {.key = std::move(key), .inclusive = false},
        };
    case TokenType::GreaterEqual:
        return LogicalIndexLookup {
            .kind = LogicalIndexLookupKind::Range,
            .lower = LogicalIndexBound {.key = std::move(key), .inclusive = true},
        };
    case TokenType::LessThan:
        return LogicalIndexLookup {
            .kind = LogicalIndexLookupKind::Range,
            .upper = LogicalIndexBound {.key = std::move(key), .inclusive = false},
        };
    case TokenType::LessEqual:
        return LogicalIndexLookup {
            .kind = LogicalIndexLookupKind::Range,
            .upper = LogicalIndexBound {.key = std::move(key), .inclusive = true},
        };
    default:
        return std::nullopt;
    }
}

[[nodiscard]]
std::optional<IndexCandidate> candidate_from_binary_predicate(const BoundBinaryExpression & expression)
{
    const auto * column = as_column_ref(expression.left());
    const BoundExpression * value_expression = &expression.right();
    auto op = expression.op();

    if (column == nullptr) {
        column = as_column_ref(expression.right());
        value_expression = &expression.left();
        auto inverted = invert_comparison(expression.op());
        if (!inverted.has_value()) {
            return std::nullopt;
        }
        op = inverted.value();
    }

    if (column == nullptr) {
        return std::nullopt;
    }

    auto key = expression_to_index_key(*value_expression);
    if (!key.has_value()) {
        return std::nullopt;
    }

    auto lookup = lookup_from_comparison(op, std::move(key.value()));
    if (!lookup.has_value()) {
        return std::nullopt;
    }

    return IndexCandidate {
        .collection_id = column->collection_id(),
        .column_id = column->column_id(),
        .lookup = std::move(lookup.value()),
    };
}

[[nodiscard]]
std::optional<IndexCandidate> candidate_from_between_predicate(const BoundBetweenExpression & expression)
{
    const auto * column = as_column_ref(expression.expression());
    if (column == nullptr) {
        return std::nullopt;
    }

    auto lower = expression_to_index_key(expression.lower());
    auto upper = expression_to_index_key(expression.upper());
    if (!lower.has_value() || !upper.has_value()) {
        return std::nullopt;
    }

    return IndexCandidate {
        .collection_id = column->collection_id(),
        .column_id = column->column_id(),
        .lookup = LogicalIndexLookup {
            .kind = LogicalIndexLookupKind::Range,
            .lower = LogicalIndexBound {.key = std::move(lower.value()), .inclusive = true},
            .upper = LogicalIndexBound {.key = std::move(upper.value()), .inclusive = true},
        },
    };
}

[[nodiscard]]
std::optional<IndexCandidate> candidate_from_predicate(const BoundExpression & expression)
{
    if (expression.kind() == BoundExpressionKind::Binary) {
        return candidate_from_binary_predicate(static_cast<const BoundBinaryExpression &>(expression));
    }
    if (expression.kind() == BoundExpressionKind::Between) {
        return candidate_from_between_predicate(static_cast<const BoundBetweenExpression &>(expression));
    }
    return std::nullopt;
}

[[nodiscard]]
bool index_supports_lookup(catalog::CatalogIndexKind index_kind, LogicalIndexLookupKind lookup_kind)
{
    if (lookup_kind == LogicalIndexLookupKind::Equal) {
        return true;
    }
    return index_kind == catalog::CatalogIndexKind::BTree;
}

[[nodiscard]]
std::optional<LogicalScanIndexHint> try_make_index_hint(
    const LogicalScan & scan,
    const BoundExpression & predicate,
    const OptimizerOptions & options,
    const catalog::CatalogReader * catalog
)
{
    if (!options.enable_index_selection || catalog == nullptr || scan.index_hint().has_value()) {
        return std::nullopt;
    }

    auto candidate = candidate_from_predicate(predicate);
    if (!candidate.has_value() || candidate->collection_id != scan.collection_id()) {
        return std::nullopt;
    }

    for (const auto * index_entry : catalog->list_indexes(scan.collection_id())) {
        if (index_entry == nullptr || index_entry->column_id() != candidate->column_id) {
            continue;
        }
        if (!index_supports_lookup(index_entry->index_kind(), candidate->lookup.kind)) {
            continue;
        }

        const auto * column = catalog->find_column(index_entry->column_id());
        if (column == nullptr) {
            continue;
        }

        return LogicalScanIndexHint {
            .index_id = index_entry->id(),
            .index_name = index_entry->name(),
            .index_kind = index_entry->index_kind(),
            .column_id = index_entry->column_id(),
            .column_name = column->name(),
            .lookup = std::move(candidate->lookup),
        };
    }

    return std::nullopt;
}

[[nodiscard]]
std::optional<std::unique_ptr<BoundExpression>> simplify_boolean_unary(
    const BoundUnaryExpression & expression,
    const OptimizerOptions & options
)
{
    if (!options.enable_boolean_simplification || expression.op() != TokenType::Not) {
        return std::nullopt;
    }
    if (is_true_literal(expression.operand())) {
        return make_bool_literal(false, expression.location());
    }
    if (is_false_literal(expression.operand())) {
        return make_bool_literal(true, expression.location());
    }
    return std::nullopt;
}

[[nodiscard]]
std::optional<std::unique_ptr<BoundExpression>> simplify_boolean_binary(
    const BoundBinaryExpression & expression,
    const OptimizerOptions & options
)
{
    if (!options.enable_boolean_simplification) {
        return std::nullopt;
    }

    if (expression.op() == TokenType::And) {
        if (is_true_literal(expression.left())) {
            return expression.right().clone();
        }
        if (is_true_literal(expression.right())) {
            return expression.left().clone();
        }
        if (is_false_literal(expression.left()) || is_false_literal(expression.right())) {
            return make_bool_literal(false, expression.location());
        }
    }

    if (expression.op() == TokenType::Or) {
        if (is_false_literal(expression.left())) {
            return expression.right().clone();
        }
        if (is_false_literal(expression.right())) {
            return expression.left().clone();
        }
        if (is_true_literal(expression.left()) || is_true_literal(expression.right())) {
            return make_bool_literal(true, expression.location());
        }
    }

    return std::nullopt;
}

[[nodiscard]]
ExpressionRewriteResult rewrite_expression(const BoundExpression & expression, const OptimizerOptions & options);

[[nodiscard]]
std::vector<std::unique_ptr<BoundExpression>> rewrite_expression_list(
    const std::vector<std::unique_ptr<BoundExpression>> & expressions,
    const OptimizerOptions & options,
    bool & changed
)
{
    std::vector<std::unique_ptr<BoundExpression>> rewritten;
    rewritten.reserve(expressions.size());
    for (const auto & expression : expressions) {
        auto result = rewrite_expression(*expression, options);
        changed = changed || result.changed;
        rewritten.push_back(std::move(result.expression));
    }
    return rewritten;
}

[[nodiscard]]
ExpressionRewriteResult rewrite_expression(const BoundExpression & expression, const OptimizerOptions & options)
{
    switch (expression.kind()) {
    case BoundExpressionKind::Literal:
    case BoundExpressionKind::Null:
    case BoundExpressionKind::ColumnRef:
    case BoundExpressionKind::Function:
    case BoundExpressionKind::Wildcard:
        return ExpressionRewriteResult {expression.clone(), false};
    case BoundExpressionKind::Unary: {
        const auto & unary = static_cast<const BoundUnaryExpression &>(expression);
        auto operand = rewrite_expression(unary.operand(), options);
        auto rebuilt = std::make_unique<BoundUnaryExpression>(
            unary.op(),
            std::move(operand.expression),
            unary.type(),
            unary.location()
        );
        if (auto simplified = simplify_boolean_unary(*rebuilt, options); simplified.has_value()) {
            return ExpressionRewriteResult {std::move(simplified.value()), true};
        }
        if (auto folded = try_fold_constant(*rebuilt, options); folded.has_value()) {
            return ExpressionRewriteResult {std::move(folded.value()), true};
        }
        return ExpressionRewriteResult {std::move(rebuilt), operand.changed};
    }
    case BoundExpressionKind::Binary: {
        const auto & binary = static_cast<const BoundBinaryExpression &>(expression);
        auto left = rewrite_expression(binary.left(), options);
        auto right = rewrite_expression(binary.right(), options);
        auto rebuilt = std::make_unique<BoundBinaryExpression>(
            std::move(left.expression),
            binary.op(),
            std::move(right.expression),
            binary.type(),
            binary.location()
        );
        if (auto simplified = simplify_boolean_binary(*rebuilt, options); simplified.has_value()) {
            return ExpressionRewriteResult {std::move(simplified.value()), true};
        }
        if (auto folded = try_fold_constant(*rebuilt, options); folded.has_value()) {
            return ExpressionRewriteResult {std::move(folded.value()), true};
        }
        return ExpressionRewriteResult {std::move(rebuilt), left.changed || right.changed};
    }
    case BoundExpressionKind::Vector: {
        const auto & vector = static_cast<const BoundVectorExpression &>(expression);
        bool changed = false;
        auto elements = rewrite_expression_list(vector.elements(), options, changed);
        return ExpressionRewriteResult {
            std::make_unique<BoundVectorExpression>(std::move(elements), vector.type(), vector.location()),
            changed,
        };
    }
    case BoundExpressionKind::In: {
        const auto & in = static_cast<const BoundInExpression &>(expression);
        bool changed = false;
        auto target = rewrite_expression(in.expression(), options);
        changed = changed || target.changed;
        auto values = rewrite_expression_list(in.values(), options, changed);
        return ExpressionRewriteResult {
            std::make_unique<BoundInExpression>(std::move(target.expression), std::move(values), in.location()),
            changed,
        };
    }
    case BoundExpressionKind::Between: {
        const auto & between = static_cast<const BoundBetweenExpression &>(expression);
        auto target = rewrite_expression(between.expression(), options);
        auto lower = rewrite_expression(between.lower(), options);
        auto upper = rewrite_expression(between.upper(), options);
        return ExpressionRewriteResult {
            std::make_unique<BoundBetweenExpression>(
                std::move(target.expression),
                std::move(lower.expression),
                std::move(upper.expression),
                between.location()
            ),
            target.changed || lower.changed || upper.changed,
        };
    }
    case BoundExpressionKind::Like: {
        const auto & like = static_cast<const BoundLikeExpression &>(expression);
        auto target = rewrite_expression(like.expression(), options);
        auto pattern = rewrite_expression(like.pattern(), options);
        return ExpressionRewriteResult {
            std::make_unique<BoundLikeExpression>(
                std::move(target.expression),
                std::move(pattern.expression),
                like.location()
            ),
            target.changed || pattern.changed,
        };
    }
    case BoundExpressionKind::Cast: {
        const auto & cast = static_cast<const BoundCastExpression &>(expression);
        auto inner = rewrite_expression(cast.expression(), options);
        auto rebuilt = std::make_unique<BoundCastExpression>(
            std::move(inner.expression),
            cast.type(),
            cast.location()
        );
        if (auto folded = try_fold_constant(*rebuilt, options); folded.has_value()) {
            return ExpressionRewriteResult {std::move(folded.value()), true};
        }
        return ExpressionRewriteResult {std::move(rebuilt), inner.changed};
    }
    }

    return ExpressionRewriteResult {expression.clone(), false};
}

[[nodiscard]]
std::vector<BoundProjectionItem> rewrite_projection_items(
    const std::vector<BoundProjectionItem> & projections,
    const OptimizerOptions & options,
    bool & changed
)
{
    std::vector<BoundProjectionItem> rewritten;
    rewritten.reserve(projections.size());
    for (const auto & projection : projections) {
        auto expression = rewrite_expression(*projection.expression, options);
        changed = changed || expression.changed;
        rewritten.push_back(BoundProjectionItem {
            .expression = std::move(expression.expression),
            .alias = projection.alias,
        });
    }
    return rewritten;
}

[[nodiscard]]
std::vector<BoundOrderByItem> rewrite_order_by_items(
    const std::vector<BoundOrderByItem> & order_by,
    const OptimizerOptions & options,
    bool & changed
)
{
    std::vector<BoundOrderByItem> rewritten;
    rewritten.reserve(order_by.size());
    for (const auto & item : order_by) {
        auto expression = rewrite_expression(*item.expression, options);
        changed = changed || expression.changed;
        rewritten.push_back(BoundOrderByItem {
            .expression = std::move(expression.expression),
            .ascending = item.ascending,
        });
    }
    return rewritten;
}

[[nodiscard]]
LogicalRewriteResult rewrite_logical_once(
    const LogicalPlanNode & node,
    const OptimizerOptions & options,
    const catalog::CatalogReader * catalog
)
{
    switch (node.kind()) {
    case LogicalPlanNodeKind::Scan:
        return LogicalRewriteResult {node.clone(), false};
    case LogicalPlanNodeKind::Filter: {
        const auto & filter = static_cast<const LogicalFilter &>(node);
        auto child = rewrite_logical_once(filter.child(), options, catalog);
        auto predicate = rewrite_expression(filter.predicate(), options);
        const auto changed = child.changed || predicate.changed;
        if (options.enable_filter_elimination && is_true_literal(*predicate.expression)) {
            return LogicalRewriteResult {std::move(child.node), true};
        }
        if (child.node->kind() == LogicalPlanNodeKind::Scan) {
            const auto & scan = static_cast<const LogicalScan &>(*child.node);
            auto index_hint = try_make_index_hint(scan, *predicate.expression, options, catalog);
            if (index_hint.has_value()) {
                child.node = std::make_unique<LogicalScan>(
                    scan.database_id(),
                    scan.collection_id(),
                    scan.collection_name(),
                    std::move(index_hint.value()),
                    scan.location()
                );
                return LogicalRewriteResult {
                    std::make_unique<LogicalFilter>(
                        std::move(child.node),
                        std::move(predicate.expression),
                        filter.location()
                    ),
                    true,
                };
            }
        }
        return LogicalRewriteResult {
            std::make_unique<LogicalFilter>(
                std::move(child.node),
                std::move(predicate.expression),
                filter.location()
            ),
            changed,
        };
    }
    case LogicalPlanNodeKind::Projection: {
        const auto & projection = static_cast<const LogicalProjection &>(node);
        auto child = rewrite_logical_once(projection.child(), options, catalog);
        bool changed = child.changed;
        auto projections = rewrite_projection_items(projection.projections(), options, changed);
        return LogicalRewriteResult {
            std::make_unique<LogicalProjection>(
                std::move(child.node),
                std::move(projections),
                projection.location()
            ),
            changed,
        };
    }
    case LogicalPlanNodeKind::OrderBy: {
        const auto & order_by = static_cast<const LogicalOrderBy &>(node);
        auto child = rewrite_logical_once(order_by.child(), options, catalog);
        bool changed = child.changed;
        auto items = rewrite_order_by_items(order_by.order_by(), options, changed);
        return LogicalRewriteResult {
            std::make_unique<LogicalOrderBy>(
                std::move(child.node),
                std::move(items),
                order_by.location()
            ),
            changed,
        };
    }
    case LogicalPlanNodeKind::Limit: {
        const auto & limit = static_cast<const LogicalLimit &>(node);
        auto child = rewrite_logical_once(limit.child(), options, catalog);
        return LogicalRewriteResult {
            std::make_unique<LogicalLimit>(
                std::move(child.node),
                limit.limit(),
                limit.offset(),
                limit.location()
            ),
            child.changed,
        };
    }
    }

    return LogicalRewriteResult {node.clone(), false};
}

[[nodiscard]]
std::unique_ptr<LogicalPlanNode> optimize_logical(
    const LogicalPlanNode & node,
    const OptimizerOptions & options,
    const catalog::CatalogReader * catalog
)
{
    auto current = node.clone();
    const auto max_passes = std::max<std::size_t>(options.max_passes, 1);
    for (std::size_t pass = 0; pass < max_passes; ++pass) {
        auto rewritten = rewrite_logical_once(*current, options, catalog);
        current = std::move(rewritten.node);
        if (!rewritten.changed) {
            break;
        }
    }
    return current;
}

[[nodiscard]]
std::vector<BoundAssignment> clone_assignments(const std::vector<BoundAssignment> & assignments)
{
    std::vector<BoundAssignment> cloned;
    cloned.reserve(assignments.size());
    for (const auto & assignment : assignments) {
        cloned.push_back(BoundAssignment {
            .column = assignment.column,
            .value = assignment.value->clone(),
        });
    }
    return cloned;
}

} // namespace

Optimizer::Optimizer(OptimizerOptions options, const catalog::CatalogReader * catalog) noexcept
    : options_(options)
    , catalog_(catalog)
{
}

std::expected<std::unique_ptr<LogicalStatementPlan>, OptimizerError> Optimizer::optimize(
    std::unique_ptr<LogicalStatementPlan> plan
) const
{
    if (plan == nullptr) {
        return std::unexpected(make_error(
            OptimizerErrorCode::InvalidArgument,
            parser::ast::AstNodeLocation {},
            "cannot optimize a null statement plan"
        ));
    }

    if (!options_.enabled) {
        return plan;
    }

    switch (plan->kind()) {
    case LogicalStatementPlanKind::Query: {
        const auto & query = static_cast<const QueryPlan &>(*plan);
        return std::make_unique<QueryPlan>(optimize_logical(query.root(), options_, catalog_), query.location());
    }
    case LogicalStatementPlanKind::Update: {
        const auto & update = static_cast<const UpdatePlan &>(*plan);
        return std::make_unique<UpdatePlan>(
            optimize_logical(update.input(), options_, catalog_),
            update.database_id(),
            update.collection_id(),
            update.collection_name(),
            clone_assignments(update.assignments()),
            update.location()
        );
    }
    case LogicalStatementPlanKind::Delete: {
        const auto & del = static_cast<const DeletePlan &>(*plan);
        return std::make_unique<DeletePlan>(
            optimize_logical(del.input(), options_, catalog_),
            del.database_id(),
            del.collection_id(),
            del.collection_name(),
            del.location()
        );
    }
    case LogicalStatementPlanKind::Use:
    case LogicalStatementPlanKind::CreateDatabase:
    case LogicalStatementPlanKind::CreateCollection:
    case LogicalStatementPlanKind::CreateIndex:
    case LogicalStatementPlanKind::CreateVectorIndex:
    case LogicalStatementPlanKind::DropDatabase:
    case LogicalStatementPlanKind::DropCollection:
    case LogicalStatementPlanKind::DropIndex:
    case LogicalStatementPlanKind::DropVectorIndex:
    case LogicalStatementPlanKind::ShowDatabases:
    case LogicalStatementPlanKind::ShowCollections:
    case LogicalStatementPlanKind::ShowIndexes:
    case LogicalStatementPlanKind::ShowVectorIndexes:
    case LogicalStatementPlanKind::DescribeCollection:
    case LogicalStatementPlanKind::Insert:
        return plan;
    }

    return plan;
}

} // namespace litedb::core::optimizer
