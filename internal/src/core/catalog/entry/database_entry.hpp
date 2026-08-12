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
    
// 数据库项
class DatabaseEntry final : public CatalogEntry
{
public:
    DatabaseEntry(common::DatabaseId id, std::string name);

public:

    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId id() const noexcept;

    // 获取数据库包含的集合 ID 列表
    [[nodiscard]]
    std::span<const common::CollectionId> collection_ids() const noexcept;

    // 查找集合 ID
    [[nodiscard]]
    std::optional<common::CollectionId> find_collection_id(std::string_view collection_key) const;

    // 判断集合是否存在
    [[nodiscard]]
    bool contains_collection(std::string_view collection_key) const;

private:
    friend class litedb::core::catalog::CatalogState;

    // 添加集合
    void add_collection(std::string_view collection_key, common::CollectionId collection_id);

    // 删除集合
    void remove_collection(std::string_view collection_key);

private:
    std::vector<common::CollectionId> collection_ids_;  // 数据库包含的集合 ID 列表
    std::unordered_map<
        std::string, common::CollectionId
    > collections_by_key_;                              // 数据库包含的集合键到 ID 的映射
};

} // namespace litedb::core::catalog::entry
