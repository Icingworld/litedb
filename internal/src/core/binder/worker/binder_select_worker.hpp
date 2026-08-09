#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class SelectStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

// SELECT 语句绑定工作器
class BinderSelectWorker
{
public:
    explicit BinderSelectWorker(const BinderContext & context) noexcept;

public:
    // 绑定 SELECT 语句
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_select(
        const parser::ast::SelectStatement & statement
    );

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder
