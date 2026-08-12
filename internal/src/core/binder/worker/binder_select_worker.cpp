#include "core/binder/worker/binder_select_worker.hpp"

#include <unordered_map>
#include <vector>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/detail/catalog_resolver.hpp"
#include "core/binder/detail/expression_binder.hpp"
#include "core/common/identifier.hpp"
#include "core/parser/ast/expression/alias_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/select_statement.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser::ast;

namespace
{

// 投影别名绑定
struct ProjectionAliasBinding
{
    const ExpressionNode * expression {nullptr};
    std::size_t count {0};
};

// 投影表达式
[[nodiscard]]
const ExpressionNode & projection_expression(const ExpressionNode & item)
{
    if (item.kind() == AstNodeKind::Alias) {
        return static_cast<const AliasExpression &>(item).expression();
    }
    return item;
}

// 投影别名
[[nodiscard]]
std::optional<std::string> projection_alias(const ExpressionNode & item)
{
    if (item.kind() == AstNodeKind::Alias) {
        return static_cast<const AliasExpression &>(item).alias();
    }
    return std::nullopt;
}

// 获取投影输出名称
[[nodiscard]]
std::string projection_output_name(
    const ExpressionNode & expression,
    const std::optional<std::string> & alias,
    std::size_t projection_index
)
{
    if (alias.has_value()) {
        return *alias;
    }

    if (expression.kind() == AstNodeKind::ColumnReference) {
        return static_cast<const ColumnReferenceExpression &>(expression).column_name();
    }

    return "expr" + std::to_string(projection_index + 1);
}

// 展开通配符表达式
[[nodiscard]]
std::expected<std::vector<std::unique_ptr<BoundExpression>>, BinderError> expand_wildcard(
    const BinderContext & context,
    const WildcardExpression & expression,
    const detail::BindingCollection & collection
)
{
    if (expression.qualifier().has_value() &&
        common::normalize_identifier(expression.qualifier().value()) !=
            collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            expression.location(),
            "Wildcard qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    std::vector<std::unique_ptr<BoundExpression>> expressions;
    for (const auto & column_reference : context.catalog().list_columns(collection.collection->id())) {
        const auto & column = column_reference.get();
        expressions.push_back(
            std::make_unique<BoundColumnRefExpression>(
                column.id(),
                column.ordinal(),
                column.type()
            )
        );
    }
    return expressions;
}

// 排序别名目标
[[nodiscard]]
const ExpressionNode * order_by_alias_target(
    const ExpressionNode & expression,
    const std::unordered_map<std::string, ProjectionAliasBinding> & aliases
)
{
    if (expression.kind() != AstNodeKind::ColumnReference) {
        return nullptr;
    }

    const auto & column = static_cast<const ColumnReferenceExpression &>(expression);
    if (column.qualifier().has_value()) {
        return nullptr;
    }

    const auto it = aliases.find(common::normalize_identifier(column.column_name()));
    if (it == aliases.end()) {
        return nullptr;
    }

    return it->second.expression;
}

// 绑定排序表达式
[[nodiscard]]
std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_order_by_expression(
    detail::ExpressionBinder & expression_binder,
    const ExpressionNode & expression,
    const std::unordered_map<std::string, ProjectionAliasBinding> & aliases
)
{
    if (const auto * alias_target = order_by_alias_target(expression, aliases);
        alias_target != nullptr) {
        const auto & column = static_cast<const ColumnReferenceExpression &>(expression);
        const auto alias_key = normalize_identifier(column.column_name());
        if (aliases.at(alias_key).count > 1) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::AmbiguousAlias,
                "ORDER BY alias is ambiguous: " + column.column_name()
            ));
        }
        return expression_binder.bind(*alias_target);
    }

    return expression_binder.bind(expression);
}

} // namespace

BinderSelectWorker::BinderSelectWorker(const BinderContext & context) noexcept
    : context_(context)
{}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderSelectWorker::bind_select(
    const SelectStatement & statement
)
{
    detail::CatalogResolver resolver(context_);

    // 解析集合
    auto collection = resolver.resolve_collection(statement.collection_name());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }
    detail::ExpressionBinder expression_binder(context_, *collection);

    std::vector<BoundProjectionItem> projections;
    std::unordered_map<std::string, ProjectionAliasBinding> aliases;

    for (const auto & item : statement.select_list()) {
        if (item->kind() == AstNodeKind::Wildcard) {
            // 展开通配符表达式
            auto expanded = expand_wildcard(
                context_,
                static_cast<const WildcardExpression &>(*item),
                *collection
            );
            if (!expanded.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expanded.error()));
            }
            // 获取集合所有列
            const auto columns = context_.catalog().list_columns(collection->collection->id());
            if (expanded->size() != columns.size()) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::UnsupportedExpression,
                    "Wildcard expansion does not match collection columns"
                ));
            }
            for (std::size_t index = 0; index < expanded->size(); ++index) {
                projections.push_back(
                    BoundProjectionItem {
                        .expression = std::move((*expanded)[index]),
                        .output_name = columns[index].get().name(),
                    }
                );
            }
            continue;
        }

        const auto & expression_node = projection_expression(*item);
        auto alias = projection_alias(*item);
        auto expression = expression_binder.bind(expression_node);
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }

        if (alias.has_value()) {
            auto & binding = aliases[normalize_identifier(*alias)];
            binding.expression = &expression_node;
            ++binding.count;
        }

        auto output_name = projection_output_name(expression_node, alias, projections.size());
        projections.push_back(
            BoundProjectionItem {
                .expression = std::move(*expression),
                .output_name = std::move(output_name),
            }
        );
    }

    // 绑定条件表达式
    std::unique_ptr<BoundExpression> where;
    if (statement.where()) {
        auto bound_where = expression_binder.bind(*statement.where());
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean((*bound_where)->type())) [[unlikely]] {
            return std::unexpected(
                make_binder_error(BinderErrorCode::InvalidType, "WHERE expression must be BOOLEAN")
            );
        }
        where = std::move(*bound_where);
    }

    // 绑定排序表达式
    std::vector<BoundOrderByItem> order_by;
    for (const auto & item : statement.order_by()) {
        auto expression = bind_order_by_expression(expression_binder, *item.expression, aliases);
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }
        order_by.push_back(
            BoundOrderByItem {
                .expression = std::move(*expression),
                .ascending = item.ascending,
            }
        );
    }

    return std::make_unique<BoundSelectStatement>(
        collection->collection->id(),
        std::move(projections),
        std::move(where),
        std::move(order_by),
        statement.limit(),
        statement.offset()
    );
}

} // namespace litedb::core::binder
