#include "core/binder/worker/binder_select_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

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

    // 绑定选择列表
    std::vector<std::unique_ptr<BoundExpression>> projections;
    // 遍历选择列表，binder 不做去重，只负责绑定
    for (const auto & item : statement.select_list()) {
        if (item->kind() == AstNodeKind::Wildcard) {
            // 展开 * 为所有列
            auto expanded = helper.expand_wildcard(static_cast<const WildcardExpression &>(*item), collection.value());
            if (!expanded.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expanded.error()));
            }
            for (auto & expression : expanded.value()) {
                projections.push_back(std::move(expression));
            }
            continue;
        }

        // 不是 * ，绑定列引用
        auto expression = helper.bind_expression(*item, collection.value());
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }
        projections.push_back(std::move(expression.value()));
    }

    // 绑定条件表达式
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

    // 绑定排序列表
    std::vector<BoundOrderByItem> order_by;
    for (const auto & item : statement.order_by()) {
        auto expression = helper.bind_expression(*item.expression, collection.value());
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
