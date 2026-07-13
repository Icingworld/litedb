#pragma once

#include <string>

#include "core/common/ids.hpp"

namespace litedb::core::meta::entry
{

/**
 * @brief 目录项类型
 */
enum class MetaEntryKind
{
    Database,             ///< 数据库
    Collection,           ///< 集合
    Column,               ///< 列
    Index,                ///< 索引
    VectorIndex,          ///< 向量索引
};

/**
 * @brief 元数据项
 */
class MetaEntry
{
public:
    MetaEntry(MetaEntryKind kind, common::MetaEntryId id, std::string name);

    MetaEntry(const MetaEntry &) = delete;
    
    MetaEntry & operator=(const MetaEntry &) = delete;
    
    MetaEntry(MetaEntry &&) noexcept = default;
    
    MetaEntry & operator=(MetaEntry &&) noexcept = default;
    
    virtual ~MetaEntry() noexcept = default;

public:
    /**
     * @brief 获取元数据项类型
     * @return 元数据项类型
     */
    [[nodiscard]]
    MetaEntryKind kind() const noexcept;

    /**
     * @brief 获取元数据项原始 ID
     * @return 元数据项 ID
     */
    [[nodiscard]]
    common::MetaEntryId raw_id() const noexcept;

    /**
     * @brief 获取元数据项名称
     * @return 元数据项名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

    /**
     * @brief 获取元数据项规范化名称
     * @return 元数据项规范化名称
     */
    [[nodiscard]]
    const std::string & key() const noexcept;

private:
    MetaEntryKind kind_;             ///< 元数据项类型
    common::MetaEntryId id_;         ///< 元数据项 ID
    std::string name_;               ///< 元数据项名称
    std::string key_;                ///< 元数据项规范化名称
};

} // namespace litedb::core::meta::entry
