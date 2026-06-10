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
};

} // namespace litedb::core::binder::bound
