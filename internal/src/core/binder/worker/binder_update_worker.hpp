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

/**
 * @brief UPDATE 语句绑定工作器
 */
class BinderUpdateWorker
{
public:
    explicit BinderUpdateWorker(BinderContext & context) noexcept;

public:
    /**
     * @brief 绑定 UPDATE 语句
     * @param statement UPDATE 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_update(
        const parser::ast::UpdateStatement & statement
    );

private:
    BinderContext & context_;        ///< 绑定上下文
};

} // namespace litedb::core::binder
