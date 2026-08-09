#include "core/binder/worker/binder_worker.hpp"

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/worker/binder_create_worker.hpp"
#include "core/binder/worker/binder_delete_worker.hpp"
#include "core/binder/worker/binder_describe_worker.hpp"
#include "core/binder/worker/binder_drop_worker.hpp"
#include "core/binder/worker/binder_insert_worker.hpp"
#include "core/binder/worker/binder_select_worker.hpp"
#include "core/binder/worker/binder_show_worker.hpp"
#include "core/binder/worker/binder_update_worker.hpp"
#include "core/binder/worker/binder_use_worker.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::parser::ast;

BinderWorker::BinderWorker(const BinderContext & context)
    : context_(context)
{}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_statement(
    const StatementNode & statement
)
{
    return dispatch_statement(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_create_database_statement(const CreateDatabaseStatement & statement)
{
    return BinderCreateWorker(context_).bind_create_database(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_create_collection_statement(const CreateCollectionStatement & statement)
{
    return BinderCreateWorker(context_).bind_create_collection(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_create_index_statement(const CreateIndexStatement & statement)
{
    return BinderCreateWorker(context_).bind_create_index(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_create_vector_index_statement(const CreateVectorIndexStatement & statement)
{
    return BinderCreateWorker(context_).bind_create_vector_index(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::visit_delete_statement(
    const DeleteStatement & statement
)
{
    return BinderDeleteWorker(context_).bind_delete(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_describe_collection_statement(const DescribeCollectionStatement & statement)
{
    return BinderDescribeWorker(context_).bind_describe_collection(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_drop_database_statement(const DropDatabaseStatement & statement)
{
    return BinderDropWorker(context_).bind_drop_database(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_drop_collection_statement(const DropCollectionStatement & statement)
{
    return BinderDropWorker(context_).bind_drop_collection(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_drop_index_statement(const DropIndexStatement & statement)
{
    return BinderDropWorker(context_).bind_drop_index(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_drop_vector_index_statement(const DropVectorIndexStatement & statement)
{
    return BinderDropWorker(context_).bind_drop_vector_index(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::visit_insert_statement(
    const InsertStatement & statement
)
{
    return BinderInsertWorker(context_).bind_insert(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::visit_select_statement(
    const SelectStatement & statement
)
{
    return BinderSelectWorker(context_).bind_select(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_show_databases_statement(const ShowDatabasesStatement & statement)
{
    return BinderShowWorker(context_).bind_show_databases(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_show_collections_statement(const ShowCollectionsStatement & statement)
{
    return BinderShowWorker(context_).bind_show_collections(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_show_indexes_statement(const ShowIndexesStatement & statement)
{
    return BinderShowWorker(context_).bind_show_indexes(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderWorker::visit_show_vector_indexes_statement(const ShowVectorIndexesStatement & statement)
{
    return BinderShowWorker(context_).bind_show_vector_indexes(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::visit_update_statement(
    const UpdateStatement & statement
)
{
    return BinderUpdateWorker(context_).bind_update(statement);
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::visit_use_statement(
    const UseStatement & statement
)
{
    return BinderUseWorker(context_).bind_use(statement);
}

} // namespace litedb::core::binder
