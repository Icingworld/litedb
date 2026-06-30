#include "core/binder/worker/binder_select_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/parser/ast/expression/alias_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/select_statement.hpp"

#include <unordered_map>
#include <vector>

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser::ast;

namespace
{

/**
 * @brief 投影别名绑定
 */
struct ProjectionAliasBinding
{
    const ExpressionNode * expression {nullptr};
    std::size_t count {0};
};

/**
 * @brief 投影表达式
 * @param item 投影项
 * @return 投影表达式
 */
const ExpressionNode & projection_expression(const ExpressionNode & item)
{
    if (item.kind() == AstNodeKind::Alias) {
        return static_cast<const AliasExpression &>(item).expression();
    }
    return item;
}

/**
 * @brief 投影别名
 * @param item 投影项
 * @return 投影别名
 */
std::optional<std::string> projection_alias(const ExpressionNode & item)
{
    if (item.kind() == AstNodeKind::Alias) {
        return static_cast<const AliasExpression &>(item).alias();
    }
    return std::nullopt;
}

/**
 * @brief 排序别名目标
 * @param expression 表达式
 * @param aliases 别名绑定
 * @return 排序别名目标
 */
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

    const auto it = aliases.find(catalog::normalize_identifier(column.column()));
    if (it == aliases.end()) {
        return nullptr;
    }

    return it->second.expression;
}

/**
 * @brief 绑定排序表达式
 * @param helper 绑定助手
 * @param expression 表达式
 * @param collection 集合
 * @param aliases 别名绑定
 * @return 绑定后的表达式
 */
std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_order_by_expression(
    BinderWorkerHelper & helper,
    const ExpressionNode & expression,
    const BindingCollection & collection,
    const std::unordered_map<std::string, ProjectionAliasBinding> & aliases
)
{
    if (const auto * alias_target = order_by_alias_target(expression, aliases); alias_target != nullptr) {
        const auto alias_key = catalog::normalize_identifier(static_cast<const ColumnReferenceExpression &>(expression).column());
        if (aliases.at(alias_key).count > 1) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::AmbiguousAlias,
                expression.location(),
                "ORDER BY alias is ambiguous: " + static_cast<const ColumnReferenceExpression &>(expression).column()
            ));
        }
        return helper.bind_expression(*alias_target, collection);
    }

    return helper.bind_expression(expression, collection);
}

} // namespace

BinderSelectWorker::BinderSelectWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderSelectWorker::bind_select(
    const SelectStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    auto collection = helper.bind_collection(statement.collection(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    std::vector<BoundProjectionItem> projections;
    std::unordered_map<std::string, ProjectionAliasBinding> aliases;

    for (const auto & item : statement.select_list()) {
        if (item->kind() == AstNodeKind::Wildcard) {
            auto expanded = helper.expand_wildcard(static_cast<const WildcardExpression &>(*item), collection.value());
            if (!expanded.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expanded.error()));
            }
            for (auto & expression : expanded.value()) {
                projections.push_back(BoundProjectionItem {
                    .expression = std::move(expression),
                    .alias = std::nullopt,
                });
            }
            continue;
        }

        const auto & expression_node = projection_expression(*item);
        auto alias = projection_alias(*item);
        auto expression = helper.bind_expression(expression_node, collection.value());
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }

        if (alias.has_value()) {
            auto & binding = aliases[catalog::normalize_identifier(alias.value())];
            binding.expression = &expression_node;
            ++binding.count;
        }

        projections.push_back(BoundProjectionItem {
            .expression = std::move(expression.value()),
            .alias = std::move(alias),
        });
    }

    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = helper.bind_expression(*statement.where(), collection.value());
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean(bound_where.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                statement.where()->location(),
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(bound_where.value());
    }

    std::vector<BoundOrderByItem> order_by;
    for (const auto & item : statement.order_by()) {
        auto expression = bind_order_by_expression(
            helper,
            *item.expression,
            collection.value(),
            aliases
        );
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }
        order_by.push_back(BoundOrderByItem {
            .expression = std::move(expression.value()),
            .ascending = item.ascending,
        });
    }

    return std::make_unique<BoundSelectStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(projections),
        std::move(where),
        std::move(order_by),
        statement.limit(),
        statement.offset(),
        statement.location()
    );
}

} // namespace litedb::core::binder
