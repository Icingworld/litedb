#pragma once

#include <string>

#include "core/common/ids.hpp"

namespace litedb::core::schema
{

// 数据库模型
class DatabaseSchema
{
public:
    DatabaseSchema(common::DatabaseId database_id, std::string database_name);

public:
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    // 获取数据库名称
    [[nodiscard]]
    const std::string & database_name() const noexcept;

private:
    common::DatabaseId database_id_;
    std::string database_name_;
};

} // namespace litedb::core::schema
