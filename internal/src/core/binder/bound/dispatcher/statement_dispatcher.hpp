#pragma once

#include <utility>
#include <type_traits>

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
 * @tparam ReturnType 返回类型
 * @tparam IsConst 是否为常量
 */
template <
    typename Derived,
    typename ReturnType,
    bool IsConst
>
class BoundStatementDispatcher
{
protected:
    /**
     * @brief 引用类型
     */
    template <typename T>
    using ReferenceType = std::conditional_t<
        IsConst,
        const T &,
        T &
    >;

protected:
    /**
     * @brief 调度语句
     * @param statement 语句
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_statement(ReferenceType<BoundStatement> statement)
    {
        switch (statement.kind()) {
        case BoundStatementKind::CreateDatabase:
            return derived().visit_create_database_statement(
                static_cast<ReferenceType<BoundCreateDatabaseStatement>>(statement)
            );
        case BoundStatementKind::CreateCollection:
            return derived().visit_create_collection_statement(
                static_cast<ReferenceType<BoundCreateCollectionStatement>>(statement)
            );
        case BoundStatementKind::CreateIndex:
            return derived().visit_create_index_statement(
                static_cast<ReferenceType<BoundCreateIndexStatement>>(statement)
            );
        case BoundStatementKind::CreateVectorIndex:
            return derived().visit_create_vector_index_statement(
                static_cast<ReferenceType<BoundCreateVectorIndexStatement>>(statement)
            );
        case BoundStatementKind::Delete:
            return derived().visit_delete_statement(
                static_cast<ReferenceType<BoundDeleteStatement>>(statement)
            );
        case BoundStatementKind::DescribeCollection:
            return derived().visit_describe_collection_statement(
                static_cast<ReferenceType<BoundDescribeCollectionStatement>>(statement)
            );
        case BoundStatementKind::DropDatabase:
            return derived().visit_drop_database_statement(
                static_cast<ReferenceType<BoundDropDatabaseStatement>>(statement)
            );
        case BoundStatementKind::DropCollection:
            return derived().visit_drop_collection_statement(
                static_cast<ReferenceType<BoundDropCollectionStatement>>(statement)
            );
        case BoundStatementKind::DropIndex:
            return derived().visit_drop_index_statement(
                static_cast<ReferenceType<BoundDropIndexStatement>>(statement)
            );
        case BoundStatementKind::DropVectorIndex:
            return derived().visit_drop_vector_index_statement(
                static_cast<ReferenceType<BoundDropVectorIndexStatement>>(statement)
            );
        case BoundStatementKind::Insert:
            return derived().visit_insert_statement(
                static_cast<ReferenceType<BoundInsertStatement>>(statement)
            );
        case BoundStatementKind::Select:
            return derived().visit_select_statement(
                static_cast<ReferenceType<BoundSelectStatement>>(statement)
            );
        case BoundStatementKind::ShowDatabases:
            return derived().visit_show_databases_statement(
                static_cast<ReferenceType<BoundShowDatabasesStatement>>(statement)
            );
        case BoundStatementKind::ShowCollections:
            return derived().visit_show_collections_statement(
                static_cast<ReferenceType<BoundShowCollectionsStatement>>(statement)
            );
        case BoundStatementKind::ShowIndexes:
            return derived().visit_show_indexes_statement(
                static_cast<ReferenceType<BoundShowIndexesStatement>>(statement)
            );
        case BoundStatementKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_statement(
                static_cast<ReferenceType<BoundShowVectorIndexesStatement>>(statement)
            );
        case BoundStatementKind::Update:
            return derived().visit_update_statement(
                static_cast<ReferenceType<BoundUpdateStatement>>(statement)
            );
        case BoundStatementKind::Use:
            return derived().visit_use_statement(
                static_cast<ReferenceType<BoundUseStatement>>(statement)
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

/**
 * @brief 常量绑定语句调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 */
template <typename Derived, typename ReturnType>
using ConstBoundStatementDispatcher = BoundStatementDispatcher<
    Derived,
    ReturnType,
    true
>;

/**
 * @brief 可变绑定语句调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 */
template <typename Derived, typename ReturnType>
using MutableBoundStatementDispatcher = BoundStatementDispatcher<
    Derived,
    ReturnType,
    false
>;

} // namespace litedb::core::binder::bound
