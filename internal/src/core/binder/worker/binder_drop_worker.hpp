#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class DropDatabaseStatement;
class DropCollectionStatement;
class DropIndexStatement;
class DropVectorIndexStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

// DROP 语句绑定工作器
class BinderDropWorker
{
public:
    explicit BinderDropWorker(const BinderContext & context) noexcept;

public:
    // 绑定 DROP DATABASE 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_drop_database(
        const parser::ast::DropDatabaseStatement & statement
    );

    // 绑定 DROP COLLECTION 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_drop_collection(
        const parser::ast::DropCollectionStatement & statement
    );

    // 绑定 DROP INDEX 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_drop_index(
        const parser::ast::DropIndexStatement & statement
    );

    // 绑定 DROP VINDEX 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_drop_vector_index(
        const parser::ast::DropVectorIndexStatement & statement
    );

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder
