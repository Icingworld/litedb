#pragma once

#include <string>

#include "core/common/ids.hpp"

namespace litedb::core::schema
{

/**
 * @brief 数据库模型
 */
class DatabaseSchema
{
public:
    DatabaseSchema(common::DatabaseId database_id, std::string database_name);

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
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
    common::DatabaseId database_id_;    // 数据库 ID
    std::string database_name_;         // 数据库名称
};

} // namespace litedb::core::schema
