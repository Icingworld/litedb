#include "core/binder/worker/binder_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/worker/binder_create_worker.hpp"
#include "core/binder/worker/binder_delete_worker.hpp"
#include "core/binder/worker/binder_describe_worker.hpp"
#include "core/binder/worker/binder_drop_worker.hpp"
#include "core/binder/worker/binder_insert_worker.hpp"
#include "core/binder/worker/binder_select_worker.hpp"
#include "core/binder/worker/binder_show_worker.hpp"
#include "core/binder/worker/binder_update_worker.hpp"
#include "core/binder/worker/binder_use_worker.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::parser::ast;

BinderWorker::BinderWorker(const BinderContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_statement(const StatementNode & statement)
{
    switch (statement.kind()) {
    case AstNodeKind::Use:
        return BinderUseWorker(context_).bind_use(static_cast<const UseStatement &>(statement));
    case AstNodeKind::CreateDatabase:
        return BinderCreateWorker(context_).bind_create_database(static_cast<const CreateDatabaseStatement &>(statement));
    case AstNodeKind::CreateCollection:
        return BinderCreateWorker(context_).bind_create_collection(static_cast<const CreateCollectionStatement &>(statement));
    case AstNodeKind::CreateIndex:
        return BinderCreateWorker(context_).bind_create_index(static_cast<const CreateIndexStatement &>(statement));
    case AstNodeKind::CreateVectorIndex:
        return BinderCreateWorker(context_).bind_create_vector_index(static_cast<const CreateVectorIndexStatement &>(statement));
    case AstNodeKind::DropDatabase:
        return BinderDropWorker(context_).bind_drop_database(static_cast<const DropDatabaseStatement &>(statement));
    case AstNodeKind::DropCollection:
        return BinderDropWorker(context_).bind_drop_collection(static_cast<const DropCollectionStatement &>(statement));
    case AstNodeKind::DropIndex:
        return BinderDropWorker(context_).bind_drop_index(static_cast<const DropIndexStatement &>(statement));
    case AstNodeKind::DropVectorIndex:
        return BinderDropWorker(context_).bind_drop_vector_index(static_cast<const DropVectorIndexStatement &>(statement));
    case AstNodeKind::Show:
        return BinderShowWorker(context_).bind_show(static_cast<const ShowStatement &>(statement));
    case AstNodeKind::ShowIndexes:
        return BinderShowWorker(context_).bind_show_indexes(static_cast<const ShowIndexesStatement &>(statement));
    case AstNodeKind::ShowVectorIndexes:
        return BinderShowWorker(context_).bind_show_vector_indexes(static_cast<const ShowVectorIndexesStatement &>(statement));
    case AstNodeKind::Describe:
        return BinderDescribeWorker(context_).bind_describe(static_cast<const DescribeStatement &>(statement));
    case AstNodeKind::Insert:
        return BinderInsertWorker(context_).bind_insert(static_cast<const InsertStatement &>(statement));
    case AstNodeKind::Select:
        return BinderSelectWorker(context_).bind_select(static_cast<const SelectStatement &>(statement));
    case AstNodeKind::Update:
        return BinderUpdateWorker(context_).bind_update(static_cast<const UpdateStatement &>(statement));
    case AstNodeKind::Delete:
        return BinderDeleteWorker(context_).bind_delete(static_cast<const DeleteStatement &>(statement));
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedStatement,
            statement.location(),
            "Unsupported statement"
        ));
    }
}

} // namespace litedb::core::binder
