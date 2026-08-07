#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 DROP DATABASE 语句
 */
class BoundDropDatabaseStatement final : public BoundStatement
{
public:
    BoundDropDatabaseStatement(
        std::optional<common::DatabaseId> database_id
    ) noexcept;

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;        // 数据库 ID
};

} // namespace litedb::core::binder::bound
