#pragma once

#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief USE 语句节点
 * @details 示例：USE database_name
 */
class BoundUseStatement final : public BoundStatement
{
public:
    BoundUseStatement(common::DatabaseId database_id, std::string database_name, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::string & database_name() const noexcept;

private:
    common::DatabaseId database_id_;        ///< 数据库ID
    std::string database_name_;             ///< 数据库名称
};

} // namespace litedb::core::binder::bound
