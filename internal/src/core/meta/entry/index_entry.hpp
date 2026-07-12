#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/meta/entry/meta_entry.hpp"

namespace litedb::core::meta::entry
{

/**
 * @brief 索引类型
 */
enum class IndexKind
{
    Hash,                 ///< 哈希索引
    BTree,                ///< B+ 树索引
};

/**
 * @brief 索引项
 */
class IndexEntry final : public MetaEntry
{
public:
    IndexEntry(
        common::IndexId id,
        common::CollectionId collection_id,
        std::vector<common::ColumnId> column_ids,
        std::string name,
        IndexKind kind,
        bool unique
    );

public:
    /**
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    common::IndexId id() const noexcept;

    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    IndexKind kind() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取首个列 ID
     * @return 首个列 ID
     */
    [[nodiscard]]
    std::optional<common::ColumnId> column_id() const noexcept;
    
    /**
     * @brief 获取列 ID 列表
     * @return 列 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::ColumnId> & column_ids() const noexcept;
    
    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

private:
    common::CollectionId collection_id_;            ///< 集合 ID
    std::vector<common::ColumnId> column_ids_;      ///< 列 ID 列表
    IndexKind kind_;                                ///< 索引类型
    bool unique_;                                   ///< 是否唯一
};

} // namespace litedb::core::meta::entry
