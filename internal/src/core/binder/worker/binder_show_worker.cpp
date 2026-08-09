#include "core/binder/worker/binder_show_worker.hpp"

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/binder/detail/catalog_resolver.hpp"
#include "core/parser/ast/statement/show_collections_statement.hpp"
#include "core/parser/ast/statement/show_databases_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderShowWorker::BinderShowWorker(const BinderContext & context) noexcept
    : context_(context)
{}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_databases(
    const ShowDatabasesStatement & statement
)
{
    return std::make_unique<BoundShowDatabasesStatement>();
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_collections(
    const ShowCollectionsStatement & statement
)
{
    detail::CatalogResolver resolver(context_);

    // 如果用户指定了数据库名称，则查找数据库
    if (statement.database_name().has_value()) {
        const auto * database = context_.meta().find_database(statement.database_name().value());
        if (database == nullptr) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DatabaseNotFound,
                "Database not found: " + statement.database_name().value()
            ));
        }

        return std::make_unique<BoundShowCollectionsStatement>(database->id());
    }

    // 如果用户没有指定数据库名称，则获取当前会话数据库
    auto database_id = resolver.require_database();
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    return std::make_unique<BoundShowCollectionsStatement>(*database_id);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderShowWorker::bind_show_indexes(
    const ShowIndexesStatement & statement
)
{
    detail::CatalogResolver resolver(context_);

    // 解析集合
    auto collection = resolver.resolve_collection(statement.collection_name());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<BoundShowIndexesStatement>(collection->collection->id());
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderShowWorker::bind_show_vector_indexes(const ShowVectorIndexesStatement & statement)
{
    detail::CatalogResolver resolver(context_);

    // 解析集合
    auto collection = resolver.resolve_collection(statement.collection_name());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<BoundShowVectorIndexesStatement>(collection->collection->id());
}

} // namespace litedb::core::binder
