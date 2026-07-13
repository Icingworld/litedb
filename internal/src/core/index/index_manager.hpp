#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/meta/meta.hpp"
#include "core/common/ids.hpp"
#include "core/index/index_error.hpp"
#include "core/index/scalar_index.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/schema/collection.hpp"
#include "core/schema/record.hpp"

namespace litedb::core::storage
{

class StorageEngine;

} // namespace litedb::core::storage

namespace litedb::core::index
{

struct IndexKeyBinding
{
    common::IndexId index_id;
    ScalarIndexKey key;
};

struct IndexUpdateBinding
{
    common::IndexId index_id;
    std::optional<ScalarIndexKey> old_key;
    std::optional<ScalarIndexKey> new_key;
    bool key_changed {false};
};

using IndexKeyBindings = std::vector<IndexKeyBinding>;
using IndexUpdateBindings = std::vector<IndexUpdateBinding>;

struct ManagedIndexView
{
    common::IndexId index_id;
    common::CollectionId collection_id;
    common::ColumnId column_id;
    std::size_t column_ordinal;
    IndexKind kind;
    bool unique {false};
    const ScalarIndex & index;
};

class IndexManager
{
public:
    IndexManager() = default;

public:
    /**
     * @brief 创建索引
     * @param index_entry 索引条目
     * @param collection_schema 集合模式
     * @param storage 存储
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IndexError> create_index(
        const meta::entry::IndexEntry & index_entry,
        const schema::CollectionSchema & collection_schema,
        const storage::StorageEngine & storage
    );

    /**
     * @brief 删除索引
     * @param index_id 索引 ID
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IndexError> drop_index(common::IndexId index_id);

    /**
     * @brief 删除集合下的所有索引
     * @param collection_id 集合 ID
     */
    void drop_collection_indexes(common::CollectionId collection_id);

    /**
     * @brief 重建所有索引
     * @param catalog 目录读取器
     * @param storage 存储管理器
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IndexError> rebuild_all(
        const meta::MetaEngine & catalog,
        const storage::StorageEngine & storage
    );

    /**
     * @brief 准备插入操作的索引键绑定
     * @param collection_id 集合 ID
     * @param record_data 记录数据
     * @return 索引键绑定
     */
    [[nodiscard]]
    std::expected<IndexKeyBindings, IndexError> prepare_insert(
        common::CollectionId collection_id,
        const schema::RecordData & record_data
    ) const;

    /**
     * @brief 执行插入操作的索引键绑定
     * @param record_id 记录 ID
     * @param bindings 索引键绑定
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IndexError> on_insert(
        common::RecordId record_id,
        const IndexKeyBindings & bindings
    );

    /**
     * @brief 准备更新操作的索引键绑定
     * @param collection_id 集合 ID
     * @param old_record_data 旧记录数据
     * @param new_record_data 新记录数据
     * @return 索引键绑定
     */
    [[nodiscard]]
    std::expected<IndexUpdateBindings, IndexError> prepare_update(
        common::CollectionId collection_id,
        const schema::RecordData & old_record_data,
        const schema::RecordData & new_record_data
    ) const;

    /**
     * @brief 执行更新操作的索引键绑定
     * @param record_id 记录 ID
     * @param bindings 索引键绑定
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IndexError> on_update(
        common::RecordId record_id,
        const IndexUpdateBindings & bindings
    );

    /**
     * @brief 准备删除操作的索引键绑定
     * @param collection_id 集合 ID
     * @param old_record_data 旧记录数据
     * @return 索引键绑定
     */
    [[nodiscard]]
    std::expected<IndexKeyBindings, IndexError> prepare_delete(
        common::CollectionId collection_id,
        const schema::RecordData & old_record_data
    ) const;

    /**
     * @brief 执行删除操作的索引键绑定
     * @param record_id 记录 ID
     * @param bindings 索引键绑定
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IndexError> on_delete(
        common::RecordId record_id,
        const IndexKeyBindings & bindings
    );

    /**
     * @brief 查找索引
     * @param index_id 索引 ID
     * @return 索引视图
     */
    [[nodiscard]]
    std::optional<ManagedIndexView> find_index(common::IndexId index_id) const noexcept;

    /**
     * @brief 列出集合下的所有索引
     * @param collection_id 集合 ID
     * @return 索引视图列表
     */
    [[nodiscard]]
    std::vector<ManagedIndexView> list_indexes(common::CollectionId collection_id) const;

    /**
     * @brief 查找列下的所有索引
     * @param collection_id 集合 ID
     * @param column_id 列 ID
     * @return 索引视图列表
     */
    [[nodiscard]]
    std::vector<ManagedIndexView> find_indexes_for_column(
        common::CollectionId collection_id,
        common::ColumnId column_id
    ) const;

    /**
     * @brief 清空索引管理器
     */
    void clear() noexcept;

private:
    struct ManagedIndex
    {
        common::IndexId index_id;
        common::CollectionId collection_id;
        common::ColumnId column_id;
        std::size_t column_ordinal;
        IndexKind kind;
        bool unique {false};
        std::unique_ptr<ScalarIndex> index;
    };

    [[nodiscard]]
    static std::unique_ptr<ScalarIndex> make_index(meta::entry::IndexKind index_kind);

    [[nodiscard]]
    static std::expected<std::optional<ScalarIndexKey>, IndexError> make_key_from_record(
        const schema::RecordData & record_data,
        std::size_t column_ordinal
    );

    [[nodiscard]]
    static std::expected<void, IndexError> validate_unique_key(
        const ManagedIndex & managed_index,
        const ScalarIndexKey & key
    );

    [[nodiscard]]
    std::expected<void, IndexError> build_index_from_storage(
        ManagedIndex & managed_index,
        const storage::StorageEngine & storage
    ) const;

    [[nodiscard]]
    ManagedIndexView make_view(const ManagedIndex & managed_index) const noexcept;

    [[nodiscard]]
    const ManagedIndex * find_managed_index(common::IndexId index_id) const noexcept;

    [[nodiscard]]
    ManagedIndex * find_managed_index(common::IndexId index_id) noexcept;

    [[nodiscard]]
    std::vector<const ManagedIndex *> list_managed_indexes(common::CollectionId collection_id) const;

private:
    std::unordered_map<common::IndexId, ManagedIndex> indexes_by_id_;
    std::unordered_map<common::CollectionId, std::vector<common::IndexId>> indexes_by_collection_;
};

} // namespace litedb::core::index
