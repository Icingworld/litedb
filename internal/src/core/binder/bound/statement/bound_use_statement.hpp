#pragma once

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 USE 语句
 */
class BoundUseStatement final : public BoundStatement
{
public:
    BoundUseStatement(common::DatabaseId database_id) noexcept;

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;        ///< 数据库 ID
};

} // namespace litedb::core::binder::bound
