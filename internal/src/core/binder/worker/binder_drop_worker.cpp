#include "core/binder/worker/binder_drop_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderDropWorker::BinderDropWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderDropWorker::bind_drop_database(
    const DropDatabaseStatement & statement
)
{
    // 查找数据库
    const auto * database = context_.meta().find_database(
        statement.database_name()
    );
    if (database == nullptr && !statement.if_exists()) [[unlikely]] {
        // 数据库不存在，且用户未指定 if_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            "Database not found: " + statement.database_name()
        ));
    }

    return std::make_unique<BoundDropDatabaseStatement>(
        database == nullptr ?
            std::nullopt : std::optional<DatabaseId>(database->id())
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderDropWorker::bind_drop_collection(
    const DropCollectionStatement & statement
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
    if (collection == nullptr && !statement.if_exists()) [[unlikely]] {
        // 集合不存在，且用户未指定 if_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            "Collection not found: " + statement.collection_name()
        ));
    }

    return std::make_unique<BoundDropCollectionStatement>(
        collection == nullptr ?
            std::nullopt : std::optional<CollectionId>(collection->id())
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderDropWorker::bind_drop_index(
    const DropIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    // 通过 Helper 绑定集合
    auto collection = helper.bind_collection(statement.collection_name());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找索引
    const auto * index = context_.meta().find_index(
        collection->collection->id(),
        statement.index_name()
    );
    if (index == nullptr && !statement.if_exists()) [[unlikely]] {
        // 索引不存在，且用户未指定 if_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::IndexNotFound,
            "Index not found: " + statement.index_name()
        ));
    }

    return std::make_unique<BoundDropIndexStatement>(
        index == nullptr ?
            std::nullopt : std::optional<IndexId>(index->id())
    );
}


std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderDropWorker::bind_drop_vector_index(
    const DropVectorIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    // 通过 Helper 绑定集合
    auto collection = helper.bind_collection(statement.collection_name());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找向量索引
    const auto * vector_index = context_.meta().find_vector_index(
        collection->collection->id(),
        statement.vector_index_name()
    );
    if (vector_index == nullptr && !statement.if_exists()) [[unlikely]] {
        // 向量索引不存在，且用户未指定 if_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::VectorIndexNotFound,
            "Vector index not found: " + statement.vector_index_name()
        ));
    }

    return std::make_unique<BoundDropVectorIndexStatement>(
        vector_index == nullptr ?
            std::nullopt : std::optional<VIndexId>(vector_index->id())
    );
}

} // namespace litedb::core::binder
