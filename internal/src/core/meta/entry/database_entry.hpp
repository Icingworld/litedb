#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/meta/entry/meta_entry.hpp"

namespace litedb::core::meta
{
class CatalogState;
}

namespace litedb::core::meta::entry
{
    
/**
 * @brief 数据库项
 */
class DatabaseEntry final : public MetaEntry
{
public:
    DatabaseEntry(common::DatabaseId id, std::string name);

public:

    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId id() const noexcept;

    /**
     * @brief 获取数据库包含的集合 ID 列表
     * @return 数据库包含的集合 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::CollectionId> & collection_ids() const noexcept;

    /**
     * @brief 查找集合 ID
     * @param collection_key 集合键
     * @return 集合 ID
     */
    [[nodiscard]]
    std::optional<common::CollectionId> find_collection_id(std::string_view collection_key) const;

    /**
     * @brief 判断集合是否存在
     * @param collection_key 集合键
     * @return 集合是否存在
     */
    [[nodiscard]]
    bool contains_collection(std::string_view collection_key) const;

private:
    friend class litedb::core::meta::CatalogState;

    /**
     * @brief 添加集合
     * @param collection_key 集合键
     * @param collection_id 集合 ID
     */
    void add_collection(std::string_view collection_key, common::CollectionId collection_id);

    /**
     * @brief 删除集合
     * @param collection_key 集合键
     * @param collection_id 集合 ID
     */
    void remove_collection(std::string_view collection_key);

private:
    std::vector<common::CollectionId> collection_ids_;  ///< 数据库包含的集合 ID 列表
    std::unordered_map<
        std::string, common::CollectionId
    > collections_by_key_;                              ///< 数据库包含的集合键到 ID 的映射
};

} // namespace litedb::core::meta::entry
