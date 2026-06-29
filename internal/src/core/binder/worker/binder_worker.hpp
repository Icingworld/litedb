#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class StatementNode;

} // namespace litedb::core::parser::ast

namespace litedb::core::catalog
{

class CatalogReader;

} // namespace litedb::core::catalog

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

/**
 * @brief 绑定器主工作器
 */
class BinderWorker
{
public:
    explicit BinderWorker(const catalog::CatalogReader & catalog) noexcept;

public:
    /**
     * @brief 绑定语句
     * @param node AST 节点
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind(
        const parser::ast::StatementNode & node
    ) const;

private:
    const catalog::CatalogReader & catalog_;    ///< 数据库读取器
};

} // namespace litedb::core::binder
