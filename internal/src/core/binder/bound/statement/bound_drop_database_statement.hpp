#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 DROP DATABASE 语句
class BoundDropDatabaseStatement final : public BoundStatement
{
public:
    BoundDropDatabaseStatement(std::optional<common::DatabaseId> database_id) noexcept;

public:
    // 获取数据库 ID
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

private:
    // database_id_ 为 nullopt 时表示用户传入了重复数据库名但是用了 IF EXISTS
    std::optional<common::DatabaseId> database_id_;
};

} // namespace litedb::core::binder::bound
