#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/catalog/catalog_request.hpp"

namespace litedb::core::binder::bound
{

// 绑定 CREATE COLLECTION 语句
class BoundCreateCollectionStatement final : public BoundStatement
{
public:
    BoundCreateCollectionStatement(
        common::DatabaseId database_id,
        std::optional<std::string> collection_name,
        std::vector<catalog::ColumnDefinition> columns,
        std::optional<std::string> comment
    );

public:
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    std::optional<const std::string &> collection_name() const noexcept;

    // 获取集合名称所有权
    // 调用后 collection_name() 返回 nullopt；再次调用返回 nullopt
    [[nodiscard]]
    std::optional<std::string> take_collection_name() noexcept;

    // 获取列定义列表
    [[nodiscard]]
    const std::vector<catalog::ColumnDefinition> & columns() const noexcept;

    // 获取列定义列表所有权
    // 调用后 columns() 为空；再次调用返回空列表
    [[nodiscard]]
    std::vector<catalog::ColumnDefinition> take_columns() noexcept;

    // 获取集合注释
    [[nodiscard]]
    std::optional<const std::string &> comment() const noexcept;

    // 获取集合注释所有权
    // 调用后 comment() 返回 nullopt；再次调用返回 nullopt
    [[nodiscard]]
    std::optional<std::string> take_comment() noexcept;

private:
    common::DatabaseId database_id_;
    // collection_name_ 为 nullopt 时表示用户传入了重复集合名但是用了 IF NOT EXISTS
    std::optional<std::string> collection_name_;
    std::vector<catalog::ColumnDefinition> columns_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::binder::bound
