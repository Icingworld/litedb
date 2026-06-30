#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class UseStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

/**
 * @brief USE 语句绑定工作器
 */
class BinderUseWorker
{
public:
    explicit BinderUseWorker(const BinderContext & context) noexcept;

public:
    /**
     * @brief 绑定 USE 语句
     * @param statement USE 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_use(
        const parser::ast::UseStatement & statement
    );

private:
    const BinderContext & context_;        ///< 绑定上下文
};

} // namespace litedb::core::binder
