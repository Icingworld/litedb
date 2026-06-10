#pragma once

#include <optional>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief DROP DATABASE 语句节点
 * @details 示例：DROP DATABASE [IF EXISTS] database_name
 */
class BoundDropDatabaseStatement final : public BoundStatement
{
public:
    BoundDropDatabaseStatement(
        std::optional<common::DatabaseId> database_id,
        std::string database_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::string & database_name() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;        ///< 数据库ID
    std::string database_name_;                            ///< 数据库名称
    bool if_exists_;                                       ///< 是否存在
};

} // namespace litedb::core::binder::bound
