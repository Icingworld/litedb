#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_error.hpp"
#include "core/parser/ast/dispatcher/statement_dispatcher.hpp"

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

/**
 * @brief 绑定工作器
 */
class BinderWorker
    : private parser::ast::AstStatementDispatcher<
          BinderWorker,
          std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
      >
{
    friend class parser::ast::AstStatementDispatcher<
        BinderWorker,
        std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    >;

public:
    explicit BinderWorker(const BinderContext & context);

public:
    /**
     * @brief 绑定语句
     * @param statement 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    bind_statement(
        const parser::ast::StatementNode & statement
    );

private:
    /**
     * @brief 访问 CREATE DATABASE 语句
     * @param statement CREATE DATABASE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_create_database_statement(
        const parser::ast::CreateDatabaseStatement & statement
    );

    /**
     * @brief 访问 CREATE COLLECTION 语句
     * @param statement CREATE COLLECTION 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_create_collection_statement(
        const parser::ast::CreateCollectionStatement & statement
    );

    /**
     * @brief 访问 CREATE INDEX 语句
     * @param statement CREATE INDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_create_index_statement(
        const parser::ast::CreateIndexStatement & statement
    );

    /**
     * @brief 访问 CREATE VINDEX 语句
     * @param statement CREATE VINDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_create_vector_index_statement(
        const parser::ast::CreateVectorIndexStatement & statement
    );

    /**
     * @brief 访问 DELETE 语句
     * @param statement DELETE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_delete_statement(
        const parser::ast::DeleteStatement & statement
    );

    /**
     * @brief 访问 DESCRIBE COLLECTION 语句
     * @param statement DESCRIBE COLLECTION 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_describe_collection_statement(
        const parser::ast::DescribeCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP DATABASE 语句
     * @param statement DROP DATABASE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_drop_database_statement(
        const parser::ast::DropDatabaseStatement & statement
    );

    /**
     * @brief 访问 DROP COLLECTION 语句
     * @param statement DROP COLLECTION 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_drop_collection_statement(
        const parser::ast::DropCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP INDEX 语句
     * @param statement DROP INDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_drop_index_statement(
        const parser::ast::DropIndexStatement & statement
    );

    /**
     * @brief 访问 DROP VINDEX 语句
     * @param statement DROP VINDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_drop_vector_index_statement(
        const parser::ast::DropVectorIndexStatement & statement
    );

    /**
     * @brief 访问 INSERT 语句
     * @param statement INSERT 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_insert_statement(
        const parser::ast::InsertStatement & statement
    );

    /**
     * @brief 访问 SELECT 语句
     * @param statement SELECT 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_select_statement(
        const parser::ast::SelectStatement & statement
    );

    /**
     * @brief 访问 SHOW DATABASES 语句
     * @param statement SHOW DATABASES 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_show_databases_statement(
        const parser::ast::ShowDatabasesStatement & statement
    );

    /**
     * @brief 访问 SHOW COLLECTIONS 语句
     * @param statement SHOW COLLECTIONS 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_show_collections_statement(
        const parser::ast::ShowCollectionsStatement & statement
    );

    /**
     * @brief 访问 SHOW INDEXES 语句
     * @param statement SHOW INDEXES 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_show_indexes_statement(
        const parser::ast::ShowIndexesStatement & statement
    );

    /**
     * @brief 访问 SHOW VINDEXES 语句
     * @param statement SHOW VINDEXES 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_show_vector_indexes_statement(
        const parser::ast::ShowVectorIndexesStatement & statement
    );

    /**
     * @brief 访问 UPDATE 语句
     * @param statement UPDATE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_update_statement(
        const parser::ast::UpdateStatement & statement
    );

    /**
     * @brief 访问 USE 语句
     * @param statement USE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    visit_use_statement(
        const parser::ast::UseStatement & statement
    );

private:
    const BinderContext & context_;         ///< 绑定上下文
};

} // namespace litedb::core::binder
