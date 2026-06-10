#pragma once

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief SHOW COLLECTIONS 语句节点
 * @details 示例：SHOW COLLECTIONS
 */
class BoundShowCollectionsStatement final : public BoundStatement
{
public:
    BoundShowCollectionsStatement(common::DatabaseId database_id, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;        ///< 数据库ID
};

} // namespace litedb::core::binder::bound
