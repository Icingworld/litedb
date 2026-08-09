#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class DeleteStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

// DELETE 语句绑定工作器
class BinderDeleteWorker
{
public:
    explicit BinderDeleteWorker(const BinderContext & context) noexcept;

public:
    // 绑定 DELETE 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_delete(
        const parser::ast::DeleteStatement & statement
    );

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder
