#include "core/binder/worker/binder_create_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/meta/meta.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderCreateWorker::BinderCreateWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderCreateWorker::bind_create_database(
    const CreateDatabaseStatement & statement
)
{
    return std::make_unique<BoundCreateDatabaseStatement>(
        statement.database_name(),
        statement.if_not_exists(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderCreateWorker::bind_create_collection(
    const CreateCollectionStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    auto database_id = helper.require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    auto columns = helper.bind_column_definitions(statement.columns());
    if (!columns.has_value()) [[unlikely]] {
        return std::unexpected(std::move(columns.error()));
    }

    return std::make_unique<BoundCreateCollectionStatement>(
        *database_id,
        statement.collection_name(),
        statement.if_not_exists(),
        std::move(*columns),
        statement.comment(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderCreateWorker::bind_create_index(
    const CreateIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);
    
    auto collection = helper.bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找列
    const auto * column = context_.meta().find_column(collection->collection->id(), statement.column_name());
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            statement.location(),
            "Column not found: " + statement.column_name()
        ));
    }

    // 检查列类型是否为向量，不允许在向量类型列上创建普通索引
    if (column->type().id == LogicalTypeId::Vector) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            statement.location(),
            "Scalar index cannot be created on VECTOR column: " + column->name()
        ));
    }

    return std::make_unique<BoundCreateIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        column->id(),
        column->name(),
        statement.index_name(),
        meta_index_kind(statement.method()),
        false,
        statement.if_not_exists(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderCreateWorker::bind_create_vector_index(
    const CreateVectorIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);
    
    auto collection = helper.bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    const auto * column = context_.meta().find_column(collection->collection->id(), statement.column_name());
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            statement.location(),
            "Column not found: " + statement.column_name()
        ));
    }

    // 检查列类型是否为向量，并且维度大于 0
    if (column->type().id != LogicalTypeId::Vector || !column->type().parameter.has_value() || column->type().parameter.value() == 0) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            statement.location(),
            "Vector index can only be created on VECTOR(n) column: " + column->name()
        ));
    }

    meta::entry::VectorDistanceMetric metric = meta::entry::VectorDistanceMetric::L2;
    switch (statement.options().metric) {
    case VectorIndexMetric::Default:
        metric = meta::entry::VectorDistanceMetric::L2;
        break;
    case VectorIndexMetric::L2:
        metric = meta::entry::VectorDistanceMetric::L2;
        break;
    case VectorIndexMetric::InnerProduct:
        metric = meta::entry::VectorDistanceMetric::InnerProduct;
        break;
    case VectorIndexMetric::Cosine:
        metric = meta::entry::VectorDistanceMetric::Cosine;
        break;
    }

    return std::make_unique<BoundCreateVectorIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        column->id(),
        column->name(),
        statement.index_name(),
        meta::entry::VectorIndexKind::Hnsw,
        metric,
        statement.options().max_neighbors.value_or(16),
        statement.options().ef_construction.value_or(200),
        statement.options().ef_search.value_or(64),
        statement.options().random_seed.value_or(0),
        statement.if_not_exists(),
        statement.location()
    );
}

} // namespace litedb::core::binder
