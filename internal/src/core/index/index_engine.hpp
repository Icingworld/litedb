#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/index/index_error.hpp"
#include "core/index/index_store.hpp"
#include "core/catalog/catalog_viewer.hpp"
#include "core/schema/collection.hpp"
#include "core/common/record.hpp"

namespace litedb::core::storage
{

class StorageEngine;

} // namespace litedb::core::storage

namespace litedb::core::index
{

// 索引键绑定
struct IndexKeyBinding
{
    common::IndexId index_id;               // 索引 ID
    ScalarIndexKey key;                     // 索引键
};

// 索引更新绑定
struct IndexUpdateBinding
{
    common::IndexId index_id;               // 索引 ID
    std::optional<ScalarIndexKey> old_key;  // 旧索引键
    std::optional<ScalarIndexKey> new_key;  // 新索引键
    bool key_changed {false};               // 是否键发生变化
};

using IndexKeyBindings = std::vector<IndexKeyBinding>;
using IndexUpdateBindings = std::vector<IndexUpdateBinding>;

// 管理索引视图
struct ManagedIndexView
{
    common::IndexId index_id;               // 索引 ID
    common::CollectionId collection_id;     // 集合 ID
    common::ColumnId column_id;             // 列 ID
    std::size_t column_ordinal;             // 列序号
    common::LogicalType key_type;           // 键类型
    IndexKind kind;                         // 索引类型
    bool unique {false};                    // 是否唯一
    std::size_t entry_count {0};            // 索引条目数量
};

// 标量索引引擎
// 管理多个 IndexStore，负责索引生命周期、记录键提取和跨索引写入编排。
class IndexEngine
{
public:
    IndexEngine(std::filesystem::path data_directory, filesystem::FileSystem & filesystem) noexcept;

    IndexEngine(const IndexEngine &) = delete;
    IndexEngine & operator=(const IndexEngine &) = delete;
    IndexEngine(IndexEngine &&) noexcept = default;
    IndexEngine & operator=(IndexEngine &&) noexcept = default;

public:
    // 创建索引
    [[nodiscard]]
    std::expected<void, IndexError> create_index(
        const catalog::entry::IndexEntry & index_entry,
        const schema::CollectionSchema & collection_schema,
        const storage::StorageEngine & storage
    );

    // 删除索引
    [[nodiscard]]
    std::expected<void, IndexError> drop_index(common::IndexId index_id);

    // 删除集合所有索引
    [[nodiscard]]
    std::expected<void, IndexError> drop_collection_indexes(common::CollectionId collection_id);

    // 从索引目录恢复所有索引
    // 持久化 BTREE 直接打开索引文件。
    [[nodiscard]]
    std::expected<void, IndexError> restore_all(
        const catalog::CatalogViewer & catalog,
        const storage::StorageEngine & storage
    );

    // 从正式索引文件原子刷新一个集合的全部标量索引
    [[nodiscard]]
    std::expected<void, IndexError> reload_collection(
        const catalog::CatalogViewer & catalog,
        const storage::StorageEngine & storage,
        common::CollectionId collection_id
    );

    // 准备插入
    [[nodiscard]]
    std::expected<IndexKeyBindings, IndexError> prepare_insert(
        common::CollectionId collection_id,
        const common::RecordData & record_data
    ) const;

    // 执行插入
    [[nodiscard]]
    std::expected<void, IndexError> on_insert(common::RecordId record_id, const IndexKeyBindings & bindings);

    // 准备更新
    [[nodiscard]]
    std::expected<IndexUpdateBindings, IndexError> prepare_update(
        common::CollectionId collection_id,
        const common::RecordData & old_record_data,
        const common::RecordData & new_record_data
    ) const;

    // 执行更新
    [[nodiscard]]
    std::expected<void, IndexError> on_update(
        common::RecordId record_id,
        const IndexUpdateBindings & bindings
    );

    // 准备删除
    [[nodiscard]]
    std::expected<IndexKeyBindings, IndexError> prepare_delete(
        common::CollectionId collection_id,
        const common::RecordData & old_record_data
    ) const;

    // 执行删除
    [[nodiscard]]
    std::expected<void, IndexError> on_delete(common::RecordId record_id, const IndexKeyBindings & bindings);

    // 查找索引
    [[nodiscard]]
    std::optional<ManagedIndexView> find_index(common::IndexId index_id) const noexcept;

    // 列出集合所有索引
    [[nodiscard]]
    std::vector<ManagedIndexView> list_indexes(common::CollectionId collection_id) const;

    // 查找列所有索引
    [[nodiscard]]
    std::vector<ManagedIndexView> find_indexes_for_column(
        common::CollectionId collection_id,
        common::ColumnId column_id
    ) const;

    // 查找等于给定键的记录 ID 列表
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> find_equal(
        common::IndexId index_id,
        const ScalarIndexKey & key
    ) const;

    // 扫描范围查询
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        common::IndexId index_id,
        const IndexRange & range
    ) const;

    [[nodiscard]]
    std::expected<std::unique_ptr<ScalarIndexCursor>, IndexError> scan_range_cursor(
        common::IndexId index_id,
        const IndexRange & range
    ) const;

    // 清空所有索引
    void clear() noexcept;

private:
    // 创建新的底层索引实现
    [[nodiscard]]
    std::expected<std::unique_ptr<ScalarIndex>, IndexError> create_backend(
        const catalog::entry::IndexEntry & index_entry,
        const common::LogicalType & key_type
    );

    // 打开已有的底层索引实现
    [[nodiscard]]
    std::expected<std::unique_ptr<ScalarIndex>, IndexError> restore_backend(
        const catalog::entry::IndexEntry & index_entry,
        const common::LogicalType & key_type
    );

    // 获取索引文件路径
    [[nodiscard]]
    std::filesystem::path index_path(common::IndexId index_id) const;

    // 从记录数据创建索引键
    [[nodiscard]]
    static std::expected<std::optional<ScalarIndexKey>, IndexError> make_key_from_record(
        const common::RecordData & record_data,
        std::size_t column_ordinal,
        const common::LogicalType & key_type
    );

    // 从存储引擎构建索引
    [[nodiscard]]
    std::expected<void, IndexError> build_index_from_storage(
        IndexStore & store,
        const storage::StorageEngine & storage
    ) const;

    // 创建管理索引视图
    [[nodiscard]]
    static ManagedIndexView make_view(const IndexStore & store) noexcept;

    // 查找索引存储
    [[nodiscard]]
    const IndexStore * find_store(common::IndexId index_id) const noexcept;

    // 查找索引存储
    [[nodiscard]]
    IndexStore * find_store(common::IndexId index_id) noexcept;

    // 列出集合所有索引存储
    [[nodiscard]]
    std::vector<const IndexStore *> list_stores(common::CollectionId collection_id) const;

private:
    std::filesystem::path data_directory_;                             // 数据库数据目录
    filesystem::FileSystem * filesystem_ {nullptr};                    // 非拥有型文件系统
    std::unordered_map<common::IndexId, IndexStore> stores_by_id_;   // 索引存储按 ID 索引
    std::unordered_map<
        common::CollectionId, std::vector<common::IndexId>
    > indexes_by_collection_;                                        // 集合索引按集合 ID 索引
};

} // namespace litedb::core::index
