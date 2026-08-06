#include "core/binder/worker/binder_create_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
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

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderCreateWorker::bind_create_database(
    const CreateDatabaseStatement & statement
)
{
    // 查找数据库
    const auto * database = context_.meta().find_database(
        statement.database_name()
    );
    if (database != nullptr && !statement.if_not_exists()) [[unlikely]] {
        // 数据库已存在，且用户未指定 if_not_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseAlreadyExists,
            "Database already exists: " + statement.database_name()
        ));
    }

    return std::make_unique<BoundCreateDatabaseStatement>(
        database == nullptr ?
            std::optional<std::string>(statement.database_name()) : std::nullopt
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderCreateWorker::bind_create_collection(
    const CreateCollectionStatement & statement
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
    if (collection != nullptr && !statement.if_not_exists()) [[unlikely]] {
        // 集合已存在，且用户未指定 if_not_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionAlreadyExists,
            "Collection already exists: " + statement.collection_name()
        ));
    }

    auto columns = helper.bind_column_definitions(statement.columns());
    if (!columns.has_value()) [[unlikely]] {
        return std::unexpected(std::move(columns.error()));
    }

    return std::make_unique<BoundCreateCollectionStatement>(
        *database_id,
        collection == nullptr ?
            std::optional<std::string>(statement.collection_name()) : std::nullopt,
        std::move(*columns),
        statement.comment()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderCreateWorker::bind_create_index(
    const CreateIndexStatement & statement
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
    if (index != nullptr && !statement.if_not_exists()) [[unlikely]] {
        // 索引已存在，且用户未指定 if_not_exists 选项
        return std::unexpected(make_binder_error(
            BinderErrorCode::IndexAlreadyExists,
            "Index already exists: " + statement.index_name()
        ));
    }

    // 查找列
    const auto * column = context_.meta().find_column(
        collection->collection->id(),
        statement.column_name()
    );
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            "Column not found: " + statement.column_name()
        ));
    }

    // 检查列类型是否为向量，不允许在向量类型列上创建普通索引
    if (column->type().id == LogicalTypeId::Vector) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            "Scalar index cannot be created on VECTOR column: " + column->name()
        ));
    }

    // 绑定索引类型
    meta::entry::IndexKind index_kind = meta::entry::IndexKind::BTree;
    switch (statement.method()) {
    case CreateIndexMethod::Default:
        [[fallthrough]];
    case CreateIndexMethod::BTree:
        index_kind = meta::entry::IndexKind::BTree;
        break;
    }

    return std::make_unique<BoundCreateIndexStatement>(
        column->id(),
        index == nullptr ?
            std::optional<std::string>(statement.index_name()) : std::nullopt,
        index_kind,
        statement.unique()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderCreateWorker::bind_create_vector_index(
    const CreateVectorIndexStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    // 通过 Helper 绑定集合
    auto collection = helper.bind_collection(statement.collection_name());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找向量索引
    const auto * index = context_.meta().find_vector_index(
        collection->collection->id(),
        statement.index_name()
    );
    if (index != nullptr && !statement.if_not_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::VectorIndexAlreadyExists,
            "Vector index already exists: " + statement.index_name()
        ));
    }

    // 查找列
    const auto * column = context_.meta().find_column(
        collection->collection->id(),
        statement.column_name()
    );
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            "Column not found: " + statement.column_name()
        ));
    }

    // 检查列类型是否为向量，并且维度大于 0
    // 理论上不会出现 dimension <= 0 的情况
    if (column->type().id != LogicalTypeId::Vector
        || !column->type().parameter.has_value()
        || column->type().parameter.value() == 0) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            "Vector index can only be created on VECTOR(n) column: " + column->name()
        ));
    }

    // 验证向量索引选项
    const auto max_neighbors = statement.options().max_neighbors.value_or(16);
    const auto ef_construction = statement.options().ef_construction.value_or(200);
    const auto ef_search = statement.options().ef_search.value_or(64);
    if (max_neighbors == 0
        || ef_construction < max_neighbors
        || ef_search == 0) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidIndexOptions,
            "HNSW options require max_neighbors > 0, "
            "ef_construction >= max_neighbors, and ef_search > 0"
        ));
    }

    // 绑定向量距离度量
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
        column->id(),
        index == nullptr ?
            std::optional<std::string>(statement.index_name()) : std::nullopt,
        meta::entry::VectorIndexKind::Hnsw,
        metric,
        max_neighbors,
        ef_construction,
        ef_search,
        statement.options().random_seed.value_or(0)
    );
}

} // namespace litedb::core::binder
