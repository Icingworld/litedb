#include "core/binder/worker/binder_drop_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/meta/meta.hpp"
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

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderDropWorker::bind_drop_database(
    const DropDatabaseStatement & statement
)
{
    const auto * database = context_.meta().find_database(statement.database_name());
    if (database == nullptr && !statement.if_exists()) {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            statement.location(),
            "Database not found: " + statement.database_name()
        ));
    }

    return std::make_unique<BoundDropDatabaseStatement>(
        database == nullptr ? std::nullopt : std::optional<DatabaseId>(database->id()),
        statement.database_name(),
        statement.if_exists(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderDropWorker::bind_drop_collection(
    const DropCollectionStatement & statement
)
{
    BinderWorkerHelper helper(context_);
    
    auto database_id = helper.require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    const auto * collection = context_.meta().find_collection(*database_id, statement.collection_name());
    if (collection == nullptr && !statement.if_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            statement.location(),
            "Collection not found: " + statement.collection_name()
        ));
    }

    return std::make_unique<BoundDropCollectionStatement>(
        *database_id,
        collection == nullptr ? std::nullopt : std::optional<CollectionId>(collection->id()),
        statement.collection_name(),
        statement.if_exists(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderDropWorker::bind_drop_index(
    const DropIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);
    
    auto collection = helper.bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找索引
    const auto * index = context_.meta().find_index(collection->collection->id(), statement.index_name());
    if (index == nullptr && !statement.if_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::IndexNotFound,
            statement.location(),
            "Index not found: " + statement.index_name()
        ));
    }

    return std::make_unique<BoundDropIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        statement.index_name(),
        statement.if_exists(),
        statement.location()
    );
}


std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderDropWorker::bind_drop_vector_index(
    const DropVectorIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);
    
    auto collection = helper.bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    const auto * index = context_.meta().find_vector_index(
        collection->collection->id(),
        statement.vector_index_name()
    );
    if (index == nullptr && !statement.if_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::IndexNotFound,
            statement.location(),
            "Vector index not found: " + statement.vector_index_name()
        ));
    }

    return std::make_unique<BoundDropVectorIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        statement.vector_index_name(),
        statement.if_exists(),
        statement.location()
    );
}

} // namespace litedb::core::binder
