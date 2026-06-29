#pragma once

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief SHOW DATABASES 语句节点
 * @details 示例：SHOW DATABASES
 */
class BoundShowDatabasesStatement final : public BoundStatement
{
public:
    explicit BoundShowDatabasesStatement(parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundStatementVisitor & visitor) const override;
};

} // namespace litedb::core::binder::bound
