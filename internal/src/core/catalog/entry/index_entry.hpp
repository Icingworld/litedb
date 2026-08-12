#pragma once

#include <string>

#include "core/catalog/entry/catalog_entry.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::catalog::entry
{

// 索引类型
enum class IndexKind
{
    BTree = 0,
};

// 索引项
class IndexEntry final : public CatalogEntry
{
public:
    IndexEntry(
        common::IndexId id,
        common::CollectionId collection_id,
        common::ColumnId column_id,
        std::string name,
        IndexKind kind,
        bool unique
    );

public:
    // 获取索引 ID
    [[nodiscard]]
    common::IndexId id() const noexcept;

    // 获取索引类型
    [[nodiscard]]
    IndexKind kind() const noexcept;

    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取首个列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 是否唯一
    [[nodiscard]]
    bool unique() const noexcept;

private:
    common::CollectionId collection_id_;
    common::ColumnId column_id_;
    IndexKind kind_;
    bool unique_;
};

} // namespace litedb::core::catalog::entry
