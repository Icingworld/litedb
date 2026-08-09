#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class ShowCollectionsStatement;
class ShowDatabasesStatement;
class ShowIndexesStatement;
class ShowVectorIndexesStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

// SHOW 语句绑定工作器
class BinderShowWorker
{
public:
    explicit BinderShowWorker(const BinderContext & context) noexcept;

public:
    // 绑定 SHOW DATABASES 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show_databases(
        const parser::ast::ShowDatabasesStatement & statement
    );

    // 绑定 SHOW COLLECTIONS 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show_collections(
        const parser::ast::ShowCollectionsStatement & statement
    );

    // 绑定 SHOW INDEXES 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show_indexes(
        const parser::ast::ShowIndexesStatement & statement
    );

    // 绑定 SHOW VECTOR INDEXES 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show_vector_indexes(
        const parser::ast::ShowVectorIndexesStatement & statement
    );

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder
