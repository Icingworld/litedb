#pragma once

#include <string>

#include "core/common/ids.hpp"

namespace litedb::core::catalog::entry
{

// 目录项类型
enum class CatalogEntryKind
{
    Database,
    Collection,
    Column,
    Index,
    VectorIndex,
};

// 元数据项
class CatalogEntry
{
public:
    CatalogEntry(CatalogEntryKind kind, common::CatalogEntryId id, std::string name);

    CatalogEntry(const CatalogEntry &) = delete;
    
    CatalogEntry & operator=(const CatalogEntry &) = delete;
    
    CatalogEntry(CatalogEntry &&) noexcept = default;
    
    CatalogEntry & operator=(CatalogEntry &&) noexcept = default;
    
    virtual ~CatalogEntry() noexcept = default;

public:
    // 获取元数据项类型
    [[nodiscard]]
    CatalogEntryKind kind() const noexcept;

    // 获取元数据项原始 ID
    [[nodiscard]]
    common::CatalogEntryId raw_id() const noexcept;

    // 获取元数据项名称
    [[nodiscard]]
    const std::string & name() const noexcept;

    // 获取元数据项规范化名称
    [[nodiscard]]
    const std::string & key() const noexcept;

private:
    CatalogEntryKind kind_;
    common::CatalogEntryId id_;
    std::string name_;
    std::string key_; // 元数据项规范化名称，用于查找、大小写不敏感比较
};

} // namespace litedb::core::catalog::entry
