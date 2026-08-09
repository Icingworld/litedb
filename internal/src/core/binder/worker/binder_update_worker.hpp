#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class UpdateStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

// UPDATE 语句绑定工作器
class BinderUpdateWorker
{
public:
    explicit BinderUpdateWorker(const BinderContext & context) noexcept;

public:
    // 绑定 UPDATE 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_update(
        const parser::ast::UpdateStatement & statement
    );

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder
