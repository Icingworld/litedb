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

/**
 * @brief SELECT 语句绑定工作器
 */
class BinderSelectWorker
{
public:
    explicit BinderSelectWorker(BinderContext & context) noexcept;

public:
    /**
     * @brief 绑定 SELECT 语句
     * @param statement SELECT 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_select(
        const parser::ast::SelectStatement & statement
    );

private:
    BinderContext & context_;        ///< 绑定上下文
};

} // namespace litedb::core::binder
