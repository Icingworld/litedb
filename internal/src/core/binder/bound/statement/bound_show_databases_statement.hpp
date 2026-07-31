#pragma once

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 SHOW DATABASES 语句
 */
class BoundShowDatabasesStatement final : public BoundStatement
{
public:
    BoundShowDatabasesStatement() noexcept;
};

} // namespace litedb::core::binder::bound
