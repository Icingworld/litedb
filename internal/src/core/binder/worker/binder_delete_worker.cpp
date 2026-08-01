#include "core/binder/worker/binder_delete_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderDeleteWorker::BinderDeleteWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderDeleteWorker::bind_delete(
    const DeleteStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    // 通过 Helper 绑定集合
    auto collection = helper.bind_collection(
        statement.collection_name()
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 通过 Helper 绑定条件表达式
    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = helper.bind_expression(
            *statement.where(),
            *collection
        );
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean((*bound_where)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(*bound_where);
    }

    return std::make_unique<BoundDeleteStatement>(
        collection->collection->id(),
        std::move(where)
    );
}

} // namespace litedb::core::binder
