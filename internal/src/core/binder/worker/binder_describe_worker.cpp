#include "core/binder/worker/binder_describe_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/parser/ast/statement/describe_collection_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderDescribeWorker::BinderDescribeWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderDescribeWorker::bind_describe_collection(
    const DescribeCollectionStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    // 通过 Helper 获取当前会话数据库
    auto database_id = helper.require_database();
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    // 查找集合
    const auto * collection = context_.meta().find_collection(
        *database_id,
        statement.collection_name()
    );
    if (collection == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            "Collection not found: " + statement.collection_name()
        ));
    }

    return std::make_unique<BoundDescribeCollectionStatement>(
        collection->id()
    );
}

} // namespace litedb::core::binder
