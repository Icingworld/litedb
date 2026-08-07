#pragma once

#include <optional>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 CREATE DATABASE 语句
 */
class BoundCreateDatabaseStatement final : public BoundStatement
{
public:
    BoundCreateDatabaseStatement(
        std::optional<std::string> database_name
    ) noexcept;

public:
    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::optional<std::string> & database_name() const noexcept;

private:
    std::optional<std::string> database_name_;         // 数据库名称
};

} // namespace litedb::core::binder::bound
