#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class ShowStatement;
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

/**
 * @brief SHOW 语句绑定工作器
 */
class BinderShowWorker
{
public:
    explicit BinderShowWorker(const BinderContext & context) noexcept;

public:
    /**
     * @brief 绑定 SHOW 语句
     * @param statement SHOW 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show(
        const parser::ast::ShowStatement & statement
    );

    /**
     * @brief 绑定 SHOW INDEXES 语句
     * @param statement SHOW INDEXES 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show_indexes(
        const parser::ast::ShowIndexesStatement & statement
    );

    /**
     * @brief 绑定 SHOW VECTOR INDEXES 语句
     * @param statement SHOW VECTOR INDEXES 语句
     * @return 绑定后的语句
     */
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_show_vector_indexes(
        const parser::ast::ShowVectorIndexesStatement & statement
    );

private:
    const BinderContext & context_;        ///< 绑定上下文
};

} // namespace litedb::core::binder
