#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/meta/entry/meta_entry.hpp"

namespace litedb::core::meta::entry
{

/**
 * @brief 集合项
 */
class CollectionEntry final : public MetaEntry
{
public:
    CollectionEntry(
        common::CollectionId id,
        common::DatabaseId database_id,
        std::string name,
        std::optional<std::string> comment = std::nullopt
    );

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId id() const noexcept;

    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取列 ID 列表
     * @return 列 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::ColumnId> & column_ids() const noexcept;

    /**
     * @brief 获取索引 ID 列表
     * @return 索引 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::IndexId> & index_ids() const noexcept;

    /**
     * @brief 获取向量索引 ID 列表
     * @return 向量索引 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::VIndexId> & vector_index_ids() const noexcept;

    /**
     * @brief 获取集合注释
     * @return 集合注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

    /**
     * @brief 查找列 ID
     * @param column_key 列键
     * @return 列 ID
     */
    [[nodiscard]]
    std::optional<common::ColumnId> find_column_id(std::string_view column_key) const;

    /**
     * @brief 查找索引 ID
     * @param index_key 索引键
     * @return 索引 ID
     */
    [[nodiscard]]
    std::optional<common::IndexId> find_index_id(std::string_view index_key) const;

    /**
     * @brief 查找向量索引 ID
     * @param index_key 向量索引键
     * @return 向量索引 ID
     */
    [[nodiscard]]
    std::optional<common::VIndexId> find_vector_index_id(std::string_view index_key) const;

    /**
     * @brief 判断列是否存在
     * @param column_key 列键
     * @return 列是否存在
     */
    [[nodiscard]]
    bool contains_column(std::string_view column_key) const;

    /**
     * @brief 判断索引是否存在
     * @param index_key 索引键
     * @return 索引是否存在
     */
    [[nodiscard]]
    bool contains_index(std::string_view index_key) const;

    /**
     * @brief 判断向量索引是否存在
     * @param index_key 向量索引键
     * @return 向量索引是否存在
     */
    [[nodiscard]]
    bool contains_vector_index(std::string_view index_key) const;

    /**
     * @brief 添加列
     * @param column_key 列键
     * @param column_id 列 ID
     */
    void add_column(std::string_view column_key, common::ColumnId column_id);

    /**
     * @brief 删除列
     * @param column_key 列键
     * @param column_id 列 ID
     */
    void remove_column(std::string_view column_key, common::ColumnId column_id);

    /**
     * @brief 添加索引
     * @param index_key 索引键
     * @param index_id 索引 ID
     */
    void add_index(std::string_view index_key, common::IndexId index_id);

    /**
     * @brief 删除索引
     * @param index_key 索引键
     * @param index_id 索引 ID
     */
    void remove_index(std::string_view index_key, common::IndexId index_id);

    /**
     * @brief 添加向量索引
     * @param index_key 向量索引键
     * @param index_id 向量索引 ID
     */
    void add_vector_index(std::string_view index_key, common::VIndexId index_id);

    /**
     * @brief 删除向量索引
     * @param index_key 向量索引键
     * @param index_id 向量索引 ID
     */
    void remove_vector_index(std::string_view index_key, common::VIndexId index_id);

private:
    common::DatabaseId database_id_;                                            ///< 数据库 ID
    std::vector<common::ColumnId> column_ids_;                                  ///< 列 ID 列表
    std::vector<common::IndexId> index_ids_;                                    ///< 索引 ID 列表
    std::vector<common::VIndexId> vector_index_ids_;                            ///< 向量索引 ID 列表
    std::unordered_map<std::string, common::ColumnId> columns_by_key_;          ///< 列键到 ID 的映射
    std::unordered_map<std::string, common::IndexId> indexes_by_key_;           ///< 索引键到 ID 的映射
    std::unordered_map<std::string, common::VIndexId> vector_indexes_by_key_;   ///< 向量索引键到 ID 的映射
    std::optional<std::string> comment_;                                        ///< 集合注释
};

} // namespace litedb::core::meta::entry
