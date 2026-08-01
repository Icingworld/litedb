#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class CreateDatabaseStatement;
class CreateCollectionStatement;
class CreateIndexStatement;
class CreateVectorIndexStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

/**
 * @brief CREATE 语句绑定工作器
 */
class BinderCreateWorker
{
public:
    explicit BinderCreateWorker(const BinderContext & context) noexcept;

public:
    /**
     * @brief 绑定 CREATE DATABASE 语句
     * @param statement CREATE DATABASE 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    bind_create_database(
        const parser::ast::CreateDatabaseStatement & statement
    );

    /**
     * @brief 绑定 CREATE COLLECTION 语句
     * @param statement CREATE COLLECTION 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    bind_create_collection(
        const parser::ast::CreateCollectionStatement & statement
    );

    /**
     * @brief 绑定 CREATE INDEX 语句
     * @param statement CREATE INDEX 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    bind_create_index(
        const parser::ast::CreateIndexStatement & statement
    );

    /**
     * @brief 绑定 CREATE VINDEX 语句
     * @param statement CREATE VINDEX 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError>
    bind_create_vector_index(
        const parser::ast::CreateVectorIndexStatement & statement
    );

private:
    const BinderContext & context_;        ///< 绑定上下文
};

} // namespace litedb::core::binder
