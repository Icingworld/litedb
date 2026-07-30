#pragma once

#include <utility>

#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_collection_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_collections_statement.hpp"
#include "core/parser/ast/statement/show_databases_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 语句调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型，默认为 void
 */
template <typename Derived, typename ReturnType = void>
class StatementDispatcher
{
protected:
    /**
     * @brief 调度语句
     * @param statement 语句
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_statement(const StatementNode & statement)
    {
        switch (statement.kind()) {
        case AstNodeKind::CreateDatabase:
            return derived().visit_create_database_statement(
                static_cast<const CreateDatabaseStatement &>(statement)
            );
        case AstNodeKind::CreateCollection:
            return derived().visit_create_collection_statement(
                static_cast<const CreateCollectionStatement &>(statement)
            );
        case AstNodeKind::CreateIndex:
            return derived().visit_create_index_statement(
                static_cast<const CreateIndexStatement &>(statement)
            );
        case AstNodeKind::CreateVectorIndex:
            return derived().visit_create_vector_index_statement(
                static_cast<const CreateVectorIndexStatement &>(statement)
            );
        case AstNodeKind::Delete:
            return derived().visit_delete_statement(
                static_cast<const DeleteStatement &>(statement)
            );
        case AstNodeKind::DescribeCollection:
            return derived().visit_describe_collection_statement(
                static_cast<const DescribeCollectionStatement &>(statement)
            );
        case AstNodeKind::DropDatabase:
            return derived().visit_drop_database_statement(
                static_cast<const DropDatabaseStatement &>(statement)
            );
        case AstNodeKind::DropCollection:
            return derived().visit_drop_collection_statement(
                static_cast<const DropCollectionStatement &>(statement)
            );
        case AstNodeKind::DropIndex:
            return derived().visit_drop_index_statement(
                static_cast<const DropIndexStatement &>(statement)
            );
        case AstNodeKind::DropVectorIndex:
            return derived().visit_drop_vector_index_statement(
                static_cast<const DropVectorIndexStatement &>(statement)
            );
        case AstNodeKind::Insert:
            return derived().visit_insert_statement(
                static_cast<const InsertStatement &>(statement)
            );
        case AstNodeKind::Select:
            return derived().visit_select_statement(
                static_cast<const SelectStatement &>(statement)
            );
        case AstNodeKind::ShowDatabases:
            return derived().visit_show_databases_statement(
                static_cast<const ShowDatabasesStatement &>(statement)
            );
        case AstNodeKind::ShowCollections:
            return derived().visit_show_collections_statement(
                static_cast<const ShowCollectionsStatement &>(statement)
            );
        case AstNodeKind::ShowIndexes:
            return derived().visit_show_indexes_statement(
                static_cast<const ShowIndexesStatement &>(statement)
            );
        case AstNodeKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_statement(
                static_cast<const ShowVectorIndexesStatement &>(statement)
            );
        case AstNodeKind::Update:
            return derived().visit_update_statement(
                static_cast<const UpdateStatement &>(statement)
            );
        case AstNodeKind::Use:
            return derived().visit_use_statement(
                static_cast<const UseStatement &>(statement)
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

} // namespace litedb::core::parser::ast
