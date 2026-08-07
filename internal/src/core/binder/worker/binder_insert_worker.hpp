#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class InsertStatement;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

class BinderContext;

/**
 * @brief INSERT 语句绑定工作器
 */
class BinderInsertWorker
{
public:
    explicit BinderInsertWorker(const BinderContext & context) noexcept;

public:
    /**
     * @brief 绑定 INSERT 语句
     * @param statement INSERT 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_insert(
        const parser::ast::InsertStatement & statement
    );

private:
    const BinderContext & context_;        // 绑定上下文
};

} // namespace litedb::core::binder
