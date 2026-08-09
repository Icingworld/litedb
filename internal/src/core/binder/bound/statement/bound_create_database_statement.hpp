#pragma once

#include <optional>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

// 绑定 CREATE DATABASE 语句
class BoundCreateDatabaseStatement final : public BoundStatement
{
public:
    BoundCreateDatabaseStatement(std::optional<std::string> database_name) noexcept;

public:
    // 获取数据库名称
    [[nodiscard]]
    const std::optional<std::string> & database_name() const noexcept;

private:
    // database_name_ = nullopt 时表示用户传入了重复数据库名但是用了 IF NOT EXISTS
    std::optional<std::string> database_name_;
};

} // namespace litedb::core::binder::bound
