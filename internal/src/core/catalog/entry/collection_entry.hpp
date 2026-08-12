#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <span>

#include "core/catalog/entry/catalog_entry.hpp"

namespace litedb::core::catalog
{

class CatalogState;

} // namespace litedb::core::catalog

namespace litedb::core::catalog::entry
{

// 集合项
class CollectionEntry final : public CatalogEntry
{
public:
    CollectionEntry(
        common::CollectionId id,
        common::DatabaseId database_id,
        std::string name,
        std::optional<std::string> comment = std::nullopt
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId id() const noexcept;

    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    // 获取列 ID 列表
    [[nodiscard]]
    std::span<const common::ColumnId> column_ids() const noexcept;

    // 获取索引 ID 列表
    [[nodiscard]]
    std::span<const common::IndexId> index_ids() const noexcept;

    // 获取向量索引 ID 列表
    [[nodiscard]]
    std::span<const common::VIndexId> vector_index_ids() const noexcept;

    // 获取集合注释
    [[nodiscard]]
    std::optional<const std::string &> comment() const noexcept;

    // 查找列 ID
    [[nodiscard]]
    std::optional<common::ColumnId> find_column_id(std::string_view column_key) const;

    // 查找索引 ID
    [[nodiscard]]
    std::optional<common::IndexId> find_index_id(std::string_view index_key) const;

    // 查找向量索引 ID
    [[nodiscard]]
    std::optional<common::VIndexId> find_vector_index_id(std::string_view index_key) const;

    // 判断列是否存在
    [[nodiscard]]
    bool contains_column(std::string_view column_key) const;

    // 判断索引是否存在
    [[nodiscard]]
    bool contains_index(std::string_view index_key) const;

    // 判断向量索引是否存在
    [[nodiscard]]
    bool contains_vector_index(std::string_view index_key) const;

private:
    friend class litedb::core::catalog::CatalogState;

    // 添加列
    void add_column(std::string_view column_key, common::ColumnId column_id);

    // 删除列
    void remove_column(std::string_view column_key);

    // 添加索引
    void add_index(std::string_view index_key, common::IndexId index_id);

    // 删除索引
    void remove_index(std::string_view index_key);

    // 添加向量索引
    void add_vector_index(std::string_view index_key, common::VIndexId index_id);

    // 删除向量索引
    void remove_vector_index(std::string_view index_key);

private:
    common::DatabaseId database_id_;                                            // 数据库 ID
    std::vector<common::ColumnId> column_ids_;                                  // 列 ID 列表
    std::vector<common::IndexId> index_ids_;                                    // 索引 ID 列表
    std::vector<common::VIndexId> vector_index_ids_;                            // 向量索引 ID 列表
    std::unordered_map<std::string, common::ColumnId> columns_by_key_;          // 列键到 ID 的映射
    std::unordered_map<std::string, common::IndexId> indexes_by_key_;           // 索引键到 ID 的映射
    std::unordered_map<std::string, common::VIndexId> vector_indexes_by_key_;   // 向量索引键到 ID 的映射
    std::optional<std::string> comment_;                                        // 集合注释
};

} // namespace litedb::core::catalog::entry
