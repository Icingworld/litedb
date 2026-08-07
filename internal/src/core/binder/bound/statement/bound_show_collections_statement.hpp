#pragma once

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 SHOW COLLECTIONS 语句
 */
class BoundShowCollectionsStatement final : public BoundStatement
{
public:
    BoundShowCollectionsStatement(
        common::DatabaseId database_id
    ) noexcept;

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;        // 数据库ID
};

} // namespace litedb::core::binder::bound
