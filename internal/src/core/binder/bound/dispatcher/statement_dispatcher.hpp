#pragma once

#include <utility>

#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定语句调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型，默认为 void
 */
template <typename Derived, typename ReturnType = void>
class BoundStatementDispatcher
{
protected:
    /**
     * @brief 调度语句
     * @param statement 语句
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_statement(const BoundStatement & statement)
    {
        switch (statement.kind()) {
        case BoundStatementKind::CreateDatabase:
            return derived().visit_create_database_statement(
                static_cast<const BoundCreateDatabaseStatement &>(statement)
            );
        case BoundStatementKind::CreateCollection:
            return derived().visit_create_collection_statement(
                static_cast<const BoundCreateCollectionStatement &>(statement)
            );
        case BoundStatementKind::CreateIndex:
            return derived().visit_create_index_statement(
                static_cast<const BoundCreateIndexStatement &>(statement)
            );
        case BoundStatementKind::CreateVectorIndex:
            return derived().visit_create_vector_index_statement(
                static_cast<const BoundCreateVectorIndexStatement &>(statement)
            );
        case BoundStatementKind::Delete:
            return derived().visit_delete_statement(
                static_cast<const BoundDeleteStatement &>(statement)
            );
        case BoundStatementKind::DescribeCollection:
            return derived().visit_describe_collection_statement(
                static_cast<const BoundDescribeCollectionStatement &>(statement)
            );
        case BoundStatementKind::DropDatabase:
            return derived().visit_drop_database_statement(
                static_cast<const BoundDropDatabaseStatement &>(statement)
            );
        case BoundStatementKind::DropCollection:
            return derived().visit_drop_collection_statement(
                static_cast<const BoundDropCollectionStatement &>(statement)
            );
        case BoundStatementKind::DropIndex:
            return derived().visit_drop_index_statement(
                static_cast<const BoundDropIndexStatement &>(statement)
            );
        case BoundStatementKind::DropVectorIndex:
            return derived().visit_drop_vector_index_statement(
                static_cast<const BoundDropVectorIndexStatement &>(statement)
            );
        case BoundStatementKind::Insert:
            return derived().visit_insert_statement(
                static_cast<const BoundInsertStatement &>(statement)
            );
        case BoundStatementKind::Select:
            return derived().visit_select_statement(
                static_cast<const BoundSelectStatement &>(statement)
            );
        case BoundStatementKind::ShowDatabases:
            return derived().visit_show_databases_statement(
                static_cast<const BoundShowDatabasesStatement &>(statement)
            );
        case BoundStatementKind::ShowCollections:
            return derived().visit_show_collections_statement(
                static_cast<const BoundShowCollectionsStatement &>(statement)
            );
        case BoundStatementKind::ShowIndexes:
            return derived().visit_show_indexes_statement(
                static_cast<const BoundShowIndexesStatement &>(statement)
            );
        case BoundStatementKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_statement(
                static_cast<const BoundShowVectorIndexesStatement &>(statement)
            );
        case BoundStatementKind::Update:
            return derived().visit_update_statement(
                static_cast<const BoundUpdateStatement &>(statement)
            );
        case BoundStatementKind::Use:
            return derived().visit_use_statement(
                static_cast<const BoundUseStatement &>(statement)
            );
        default:
            std::unreachable();
        }
    }

    /**
     * @brief 调度语句
     * @param statement 语句
     * @return 返回值
     * @details 非 const 重载，允许派生类移动消费语句成员
     */
    [[nodiscard]]
    ReturnType dispatch_statement(BoundStatement & statement)
    {
        switch (statement.kind()) {
        case BoundStatementKind::CreateDatabase:
            return derived().visit_create_database_statement(
                static_cast<BoundCreateDatabaseStatement &>(statement)
            );
        case BoundStatementKind::CreateCollection:
            return derived().visit_create_collection_statement(
                static_cast<BoundCreateCollectionStatement &>(statement)
            );
        case BoundStatementKind::CreateIndex:
            return derived().visit_create_index_statement(
                static_cast<BoundCreateIndexStatement &>(statement)
            );
        case BoundStatementKind::CreateVectorIndex:
            return derived().visit_create_vector_index_statement(
                static_cast<BoundCreateVectorIndexStatement &>(statement)
            );
        case BoundStatementKind::Delete:
            return derived().visit_delete_statement(
                static_cast<BoundDeleteStatement &>(statement)
            );
        case BoundStatementKind::DescribeCollection:
            return derived().visit_describe_collection_statement(
                static_cast<BoundDescribeCollectionStatement &>(statement)
            );
        case BoundStatementKind::DropDatabase:
            return derived().visit_drop_database_statement(
                static_cast<BoundDropDatabaseStatement &>(statement)
            );
        case BoundStatementKind::DropCollection:
            return derived().visit_drop_collection_statement(
                static_cast<BoundDropCollectionStatement &>(statement)
            );
        case BoundStatementKind::DropIndex:
            return derived().visit_drop_index_statement(
                static_cast<BoundDropIndexStatement &>(statement)
            );
        case BoundStatementKind::DropVectorIndex:
            return derived().visit_drop_vector_index_statement(
                static_cast<BoundDropVectorIndexStatement &>(statement)
            );
        case BoundStatementKind::Insert:
            return derived().visit_insert_statement(
                static_cast<BoundInsertStatement &>(statement)
            );
        case BoundStatementKind::Select:
            return derived().visit_select_statement(
                static_cast<BoundSelectStatement &>(statement)
            );
        case BoundStatementKind::ShowDatabases:
            return derived().visit_show_databases_statement(
                static_cast<BoundShowDatabasesStatement &>(statement)
            );
        case BoundStatementKind::ShowCollections:
            return derived().visit_show_collections_statement(
                static_cast<BoundShowCollectionsStatement &>(statement)
            );
        case BoundStatementKind::ShowIndexes:
            return derived().visit_show_indexes_statement(
                static_cast<BoundShowIndexesStatement &>(statement)
            );
        case BoundStatementKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_statement(
                static_cast<BoundShowVectorIndexesStatement &>(statement)
            );
        case BoundStatementKind::Update:
            return derived().visit_update_statement(
                static_cast<BoundUpdateStatement &>(statement)
            );
        case BoundStatementKind::Use:
            return derived().visit_use_statement(
                static_cast<BoundUseStatement &>(statement)
            );
        default:
            std::unreachable();
        }
    }

private:
    /**
     * @brief 获取派生类引用
     * @return 派生类引用
     */
    [[nodiscard]]
    Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

} // namespace litedb::core::binder::bound
