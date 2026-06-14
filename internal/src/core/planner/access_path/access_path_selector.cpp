#include "core/planner/access_path/access_path_selector.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/common/logical_type.hpp"
#include "core/planner/logical/node/logical_index_scan.hpp"
#include "core/planner/logical/node/logical_scan.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::planner::access_path
{

namespace
{

using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundColumnRefExpression;
using binder::bound::BoundExpression;
using binder::bound::BoundExpressionKind;
using binder::bound::BoundLiteralExpression;
using common::LogicalTypeId;
using parser::TokenType;

struct CandidateLookup
{
    common::ColumnId column_id;
    logical::IndexLookup lookup;
    bool range {false};
};

[[nodiscard]]
std::unique_ptr<logical::LogicalPlanNode> make_scan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
{
    return std::make_unique<logical::LogicalScan>(
        database_id,
        collection_id,
        std::move(collection_name),
        location
    );
}

template <typename T>
[[nodiscard]]
std::optional<T> parse_number(std::string_view value)
{
    T parsed {};
    const auto * begin = value.data();
    const auto * end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc {} || result.ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]]
std::optional<schema::Value> literal_value(const BoundLiteralExpression & literal)
{
    const auto value = std::string_view {literal.value()};
    switch (literal.type().id) {
    case LogicalTypeId::Boolean:
        if (value == "true" || value == "TRUE") {
            return schema::Value {true};
        }
        if (value == "false" || value == "FALSE") {
            return schema::Value {false};
        }
        return std::nullopt;
    case LogicalTypeId::Integer: {
        auto parsed = parse_number<std::int32_t>(value);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        return schema::Value {parsed.value()};
    }
    case LogicalTypeId::BigInt: {
        auto parsed = parse_number<std::int64_t>(value);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        return schema::Value {parsed.value()};
    }
    case LogicalTypeId::Float: {
        auto parsed = parse_number<float>(value);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        return schema::Value {parsed.value()};
    }
    case LogicalTypeId::Double: {
        auto parsed = parse_number<double>(value);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        return schema::Value {parsed.value()};
    }
    case LogicalTypeId::Varchar:
        return schema::Value {literal.value()};
    case LogicalTypeId::Null:
    case LogicalTypeId::Vector:
        return std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]]
std::optional<index::ScalarIndexKey> key_from_literal(const BoundExpression & expression)
{
    if (expression.kind() != BoundExpressionKind::Literal) {
        return std::nullopt;
    }

    auto value = literal_value(static_cast<const BoundLiteralExpression &>(expression));
    if (!value.has_value() || value->is_null()) {
        return std::nullopt;
    }

    auto key = index::ScalarIndexKey::from_value(std::move(value.value()));
    if (!key.has_value()) {
        return std::nullopt;
    }
    return std::move(key.value());
}

[[nodiscard]]
const BoundColumnRefExpression * as_column(const BoundExpression & expression)
{
    if (expression.kind() != BoundExpressionKind::ColumnRef) {
        return nullptr;
    }
    return &static_cast<const BoundColumnRefExpression &>(expression);
}

[[nodiscard]]
std::optional<TokenType> reverse_operator(TokenType op)
{
    switch (op) {
    case TokenType::Equal:
        return TokenType::Equal;
    case TokenType::LessThan:
        return TokenType::GreaterThan;
    case TokenType::LessEqual:
        return TokenType::GreaterEqual;
    case TokenType::GreaterThan:
        return TokenType::LessThan;
    case TokenType::GreaterEqual:
        return TokenType::LessEqual;
    default:
        return std::nullopt;
    }
}

[[nodiscard]]
std::optional<logical::IndexLookup> lookup_from_binary(TokenType op, index::ScalarIndexKey key)
{
    switch (op) {
    case TokenType::Equal:
        return logical::IndexLookup::equal(std::move(key));
    case TokenType::LessThan:
        return logical::IndexLookup::range_scan(index::IndexRange::upper_bound(std::move(key), false));
    case TokenType::LessEqual:
        return logical::IndexLookup::range_scan(index::IndexRange::upper_bound(std::move(key), true));
    case TokenType::GreaterThan:
        return logical::IndexLookup::range_scan(index::IndexRange::lower_bound(std::move(key), false));
    case TokenType::GreaterEqual:
        return logical::IndexLookup::range_scan(index::IndexRange::lower_bound(std::move(key), true));
    default:
        return std::nullopt;
    }
}

[[nodiscard]]
std::optional<CandidateLookup> candidate_from_binary(
    common::CollectionId collection_id,
    const BoundBinaryExpression & binary
)
{
    const auto * column = as_column(binary.left());
    auto key = key_from_literal(binary.right());
    auto op = std::optional<TokenType> {binary.op()};

    if (column == nullptr || !key.has_value()) {
        column = as_column(binary.right());
        key = key_from_literal(binary.left());
        op = reverse_operator(binary.op());
    }

    if (column == nullptr || !key.has_value() || !op.has_value() || column->collection_id() != collection_id) {
        return std::nullopt;
    }

    auto lookup = lookup_from_binary(op.value(), std::move(key.value()));
    if (!lookup.has_value()) {
        return std::nullopt;
    }

    const auto is_range = lookup->kind == logical::IndexLookupKind::Range;
    return CandidateLookup {
        .column_id = column->column_id(),
        .lookup = std::move(lookup.value()),
        .range = is_range,
    };
}

[[nodiscard]]
std::optional<CandidateLookup> candidate_from_between(
    common::CollectionId collection_id,
    const BoundBetweenExpression & between
)
{
    const auto * column = as_column(between.expression());
    if (column == nullptr || column->collection_id() != collection_id) {
        return std::nullopt;
    }

    auto lower = key_from_literal(between.lower());
    auto upper = key_from_literal(between.upper());
    if (!lower.has_value() || !upper.has_value()) {
        return std::nullopt;
    }

    return CandidateLookup {
        .column_id = column->column_id(),
        .lookup = logical::IndexLookup::range_scan(index::IndexRange::closed(
            std::move(lower.value()),
            std::move(upper.value())
        )),
        .range = true,
    };
}

[[nodiscard]]
std::optional<CandidateLookup> candidate_from_predicate(
    common::CollectionId collection_id,
    const BoundExpression & predicate
)
{
    switch (predicate.kind()) {
    case BoundExpressionKind::Binary:
        return candidate_from_binary(collection_id, static_cast<const BoundBinaryExpression &>(predicate));
    case BoundExpressionKind::Between:
        return candidate_from_between(collection_id, static_cast<const BoundBetweenExpression &>(predicate));
    default:
        return std::nullopt;
    }
}

[[nodiscard]]
std::optional<index::ManagedIndexView> choose_index(
    const index::IndexManager & index_manager,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    bool range
)
{
    const auto indexes = index_manager.find_indexes_for_column(collection_id, column_id);
    if (indexes.empty()) {
        return std::nullopt;
    }

    if (!range) {
        for (const auto & candidate : indexes) {
            if (candidate.kind == index::IndexKind::Hash) {
                return candidate;
            }
        }
        for (const auto & candidate : indexes) {
            if (candidate.kind == index::IndexKind::BTree) {
                return candidate;
            }
        }
        return std::nullopt;
    }

    for (const auto & candidate : indexes) {
        if (candidate.index.supports_range_scan()) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace

AccessPathSelector::AccessPathSelector(const index::IndexManager * index_manager) noexcept
    : index_manager_(index_manager)
{
}

std::unique_ptr<logical::LogicalPlanNode> AccessPathSelector::select_scan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    const BoundExpression * predicate,
    parser::ast::AstNodeLocation location
) const
{
    if (index_manager_ == nullptr || predicate == nullptr) {
        return make_scan(database_id, collection_id, std::move(collection_name), location);
    }

    auto candidate = candidate_from_predicate(collection_id, *predicate);
    if (!candidate.has_value()) {
        return make_scan(database_id, collection_id, std::move(collection_name), location);
    }

    auto selected_index = choose_index(
        *index_manager_,
        collection_id,
        candidate->column_id,
        candidate->range
    );
    if (!selected_index.has_value()) {
        return make_scan(database_id, collection_id, std::move(collection_name), location);
    }

    return std::make_unique<logical::LogicalIndexScan>(
        database_id,
        collection_id,
        std::move(collection_name),
        selected_index->index_id,
        selected_index->kind,
        selected_index->column_id,
        std::move(candidate->lookup),
        location
    );
}

} // namespace litedb::core::planner::access_path
