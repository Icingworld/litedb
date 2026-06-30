#include "core/binder/worker/binder_show_worker.hpp"

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/parser/ast/statement/show_collections_statement.hpp"
#include "core/parser/ast/statement/show_databases_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderShowWorker::BinderShowWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_databases(
    const ShowDatabasesStatement & statement
)
{
    return std::make_unique<BoundShowDatabasesStatement>(statement.location());
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_collections(
    const ShowCollectionsStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    if (statement.database_name().has_value()) {
        const auto * database = context_.catalog().find_database(statement.database_name().value());
        if (database == nullptr) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DatabaseNotFound,
                statement.location(),
                "Database not found: " + statement.database_name().value()
            ));
        }

        return std::make_unique<BoundShowCollectionsStatement>(database->id(), statement.location());
    }

    const auto database_id = helper.require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    return std::make_unique<BoundShowCollectionsStatement>(database_id.value(), statement.location());
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_indexes(
    const ShowIndexesStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    auto collection = helper.bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<BoundShowIndexesStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_vector_indexes(
    const ShowVectorIndexesStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    auto collection = helper.bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<BoundShowVectorIndexesStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        statement.location()
    );
}

} // namespace litedb::core::binder
