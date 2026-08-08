#pragma once

#include <type_traits>
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

// AST 语句调度器
// 基于 CRTP 实现，Derived 为派生类类型，ReturnType 为返回类型
// IsConst 为是否为常量，当调度器会修改节点时，传入 false，否则传入 true
template <typename Derived, typename ReturnType, bool IsConst> class AstStatementDispatcher
{
protected:
    // 引用类型
    template <typename T> using ReferenceType = std::conditional_t<IsConst, const T &, T &>;

protected:
    // 调度语句
    [[nodiscard]]
    ReturnType dispatch_statement(ReferenceType<StatementNode> statement)
    {
        switch (statement.kind()) {
        case AstNodeKind::CreateDatabase:
            return derived().visit_create_database_statement(
                static_cast<ReferenceType<CreateDatabaseStatement>>(statement)
            );
        case AstNodeKind::CreateCollection:
            return derived().visit_create_collection_statement(
                static_cast<ReferenceType<CreateCollectionStatement>>(statement)
            );
        case AstNodeKind::CreateIndex:
            return derived().visit_create_index_statement(
                static_cast<ReferenceType<CreateIndexStatement>>(statement)
            );
        case AstNodeKind::CreateVectorIndex:
            return derived().visit_create_vector_index_statement(
                static_cast<ReferenceType<CreateVectorIndexStatement>>(statement)
            );
        case AstNodeKind::Delete:
            return derived().visit_delete_statement(
                static_cast<ReferenceType<DeleteStatement>>(statement)
            );
        case AstNodeKind::DescribeCollection:
            return derived().visit_describe_collection_statement(
                static_cast<ReferenceType<DescribeCollectionStatement>>(statement)
            );
        case AstNodeKind::DropDatabase:
            return derived().visit_drop_database_statement(
                static_cast<ReferenceType<DropDatabaseStatement>>(statement)
            );
        case AstNodeKind::DropCollection:
            return derived().visit_drop_collection_statement(
                static_cast<ReferenceType<DropCollectionStatement>>(statement)
            );
        case AstNodeKind::DropIndex:
            return derived().visit_drop_index_statement(
                static_cast<ReferenceType<DropIndexStatement>>(statement)
            );
        case AstNodeKind::DropVectorIndex:
            return derived().visit_drop_vector_index_statement(
                static_cast<ReferenceType<DropVectorIndexStatement>>(statement)
            );
        case AstNodeKind::Insert:
            return derived().visit_insert_statement(
                static_cast<ReferenceType<InsertStatement>>(statement)
            );
        case AstNodeKind::Select:
            return derived().visit_select_statement(
                static_cast<ReferenceType<SelectStatement>>(statement)
            );
        case AstNodeKind::ShowDatabases:
            return derived().visit_show_databases_statement(
                static_cast<ReferenceType<ShowDatabasesStatement>>(statement)
            );
        case AstNodeKind::ShowCollections:
            return derived().visit_show_collections_statement(
                static_cast<ReferenceType<ShowCollectionsStatement>>(statement)
            );
        case AstNodeKind::ShowIndexes:
            return derived().visit_show_indexes_statement(
                static_cast<ReferenceType<ShowIndexesStatement>>(statement)
            );
        case AstNodeKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_statement(
                static_cast<ReferenceType<ShowVectorIndexesStatement>>(statement)
            );
        case AstNodeKind::Update:
            return derived().visit_update_statement(
                static_cast<ReferenceType<UpdateStatement>>(statement)
            );
        case AstNodeKind::Use:
            return derived().visit_use_statement(
                static_cast<ReferenceType<UseStatement>>(statement)
            );
        default:
            std::unreachable();
        }
    }

private:
    // 获取派生类引用
    [[nodiscard]]
    Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

// 常量 AST 语句调度器
template <typename Derived, typename ReturnType>
using ConstAstStatementDispatcher = AstStatementDispatcher<Derived, ReturnType, true>;

} // namespace litedb::core::parser::ast
