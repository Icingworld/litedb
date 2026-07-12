#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/meta/entry/collection_entry.hpp"
#include "core/meta/entry/column_entry.hpp"
#include "core/meta/entry/database_entry.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/meta/meta_engine_error.hpp"
#include "core/meta/meta_request.hpp"
#include "core/meta/meta_snapshot.hpp"
#include "core/meta/meta_store.hpp"

namespace litedb::core::meta
{

/**
 * @brief 元数据引擎
 * @note 查询接口返回的 entry 指针仅在下一次 mutation 或 restore 前有效。
 */
class MetaEngine
{
public:
    MetaEngine() = default;

    explicit MetaEngine(MetaStore & store) noexcept;

    MetaEngine(const MetaEngine &) = delete;

    MetaEngine & operator=(const MetaEngine &) = delete;

    MetaEngine(MetaEngine &&) noexcept = default;

    MetaEngine & operator=(MetaEngine &&) noexcept = default;

public:
    /**
     * @brief 加载元数据
     * @return 结果
     * @details 从磁盘中读取并加载元数据；未绑定存储时直接成功
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> load();

    /**
     * @brief 获取元数据快照
     * @return 元数据快照
     * @details 导出当前内存中的完整元数据状态
     */
    [[nodiscard]]
    MetaSnapshot snapshot() const;

    /**
     * @brief 恢复元数据
     * @param snapshot 元数据快照
     * @return 结果
     * @details 用快照替换当前内存状态，并校验快照结构与 ID 连续性
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> restore(const MetaSnapshot & snapshot);

    /**
     * @brief 按名称查找数据库
     * @param name 数据库名
     * @return 数据库项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::DatabaseEntry * find_database(std::string_view name) const;

    /**
     * @brief 按 ID 查找数据库
     * @param id 数据库 ID
     * @return 数据库项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::DatabaseEntry * find_database(common::DatabaseId id) const;

    /**
     * @brief 在指定数据库中按名称查找集合
     * @param database_id 数据库 ID
     * @param name 集合名
     * @return 集合项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::CollectionEntry * find_collection(common::DatabaseId database_id, std::string_view name) const;

    /**
     * @brief 按 ID 查找集合
     * @param id 集合 ID
     * @return 集合项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::CollectionEntry * find_collection(common::CollectionId id) const;

    /**
     * @brief 在指定集合中按名称查找列
     * @param collection_id 集合 ID
     * @param name 列名
     * @return 列项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::ColumnEntry * find_column(common::CollectionId collection_id, std::string_view name) const;

    /**
     * @brief 按 ID 查找列
     * @param id 列 ID
     * @return 列项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::ColumnEntry * find_column(common::ColumnId id) const;

    /**
     * @brief 在指定集合中按名称查找标量索引
     * @param collection_id 集合 ID
     * @param name 索引名
     * @return 索引项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::IndexEntry * find_index(common::CollectionId collection_id, std::string_view name) const;

    /**
     * @brief 按 ID 查找标量索引
     * @param id 索引 ID
     * @return 索引项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::IndexEntry * find_index(common::IndexId id) const;

    /**
     * @brief 在指定集合中按名称查找向量索引
     * @param collection_id 集合 ID
     * @param name 向量索引名
     * @return 向量索引项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::VectorIndexEntry * find_vector_index(common::CollectionId collection_id, std::string_view name) const;

    /**
     * @brief 按 ID 查找向量索引
     * @param id 向量索引 ID
     * @return 向量索引项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    const entry::VectorIndexEntry * find_vector_index(common::VIndexId id) const;

    /**
     * @brief 列出所有数据库
     * @return 数据库项指针列表
     */
    [[nodiscard]]
    std::vector<const entry::DatabaseEntry *> list_databases() const;

    /**
     * @brief 列出指定数据库下的所有集合
     * @param database_id 数据库 ID
     * @return 集合项指针列表
     */
    [[nodiscard]]
    std::vector<const entry::CollectionEntry *> list_collections(common::DatabaseId database_id) const;

    /**
     * @brief 列出指定集合下的所有列
     * @param collection_id 集合 ID
     * @return 列项指针列表
     */
    [[nodiscard]]
    std::vector<const entry::ColumnEntry *> list_columns(common::CollectionId collection_id) const;

    /**
     * @brief 列出指定集合下的所有标量索引
     * @param collection_id 集合 ID
     * @return 索引项指针列表
     */
    [[nodiscard]]
    std::vector<const entry::IndexEntry *> list_indexes(common::CollectionId collection_id) const;

    /**
     * @brief 列出指定集合下的所有向量索引
     * @param collection_id 集合 ID
     * @return 向量索引项指针列表
     */
    [[nodiscard]]
    std::vector<const entry::VectorIndexEntry *> list_vector_indexes(common::CollectionId collection_id) const;

    /**
     * @brief 创建数据库
     * @param request 创建请求
     * @return 新数据库 ID
     */
    [[nodiscard]]
    std::expected<common::DatabaseId, MetaEngineError> create_database(const CreateDatabaseRequest & request);

    /**
     * @brief 删除数据库
     * @param request 删除请求
     * @return 结果
     * @details 会级联删除其下所有集合及相关元数据
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> drop_database(const DropDatabaseRequest & request);

    /**
     * @brief 创建集合
     * @param request 创建请求
     * @return 新集合 ID
     */
    [[nodiscard]]
    std::expected<common::CollectionId, MetaEngineError> create_collection(const CreateCollectionRequest & request);

    /**
     * @brief 删除集合
     * @param request 删除请求
     * @return 结果
     * @details 会级联删除其下所有列与索引
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> drop_collection(const DropCollectionRequest & request);

    /**
     * @brief 创建标量索引
     * @param request 创建请求
     * @return 新索引 ID
     */
    [[nodiscard]]
    std::expected<common::IndexId, MetaEngineError> create_index(const CreateIndexRequest & request);

    /**
     * @brief 删除标量索引
     * @param request 删除请求
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> drop_index(const DropIndexRequest & request);

    /**
     * @brief 创建向量索引
     * @param request 创建请求
     * @return 新向量索引 ID
     */
    [[nodiscard]]
    std::expected<common::VIndexId, MetaEngineError> create_vector_index(const CreateVectorIndexRequest & request);

    /**
     * @brief 删除向量索引
     * @param request 删除请求
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> drop_vector_index(const DropVectorIndexRequest & request);

private:
    /**
     * @brief 持久化当前元数据
     * @return 结果
     * @note 未绑定存储时直接成功
     */
    [[nodiscard]]
    std::expected<void, MetaEngineError> persist() const;

    /**
     * @brief 按 ID 查找可修改的数据库项
     * @param id 数据库 ID
     * @return 数据库项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    entry::DatabaseEntry * find_database_mutable(common::DatabaseId id);

    /**
     * @brief 按 ID 查找可修改的集合项
     * @param id 集合 ID
     * @return 集合项指针，未找到时返回 nullptr
     */
    [[nodiscard]]
    entry::CollectionEntry * find_collection_mutable(common::CollectionId id);

    /**
     * @brief 删除集合并级联清理其列与索引
     * @param id 集合 ID
     */
    void erase_collection(common::CollectionId id);

private:
    common::DatabaseId next_database_id_ {1};                                           ///< 下一个数据库 ID
    common::CollectionId next_collection_id_ {1};                                       ///< 下一个集合 ID
    common::ColumnId next_column_id_ {1};                                               ///< 下一个列 ID
    common::IndexId next_index_id_ {1};                                                 ///< 下一个索引 ID
    common::VIndexId next_vector_index_id_ {1};                                       ///< 下一个向量索引 ID
    std::vector<common::DatabaseId> database_ids_;                                    ///< 数据库 ID 顺序列表
    std::unordered_map<common::DatabaseId, std::unique_ptr<entry::DatabaseEntry>> databases_;     ///< 数据库项
    std::unordered_map<std::string, common::DatabaseId> database_keys_;                 ///< 数据库名到 ID 的映射
    std::unordered_map<common::CollectionId, std::unique_ptr<entry::CollectionEntry>> collections_;   ///< 集合项
    std::unordered_map<common::ColumnId, std::unique_ptr<entry::ColumnEntry>> columns_;             ///< 列项
    std::unordered_map<common::IndexId, std::unique_ptr<entry::IndexEntry>> indexes_;               ///< 标量索引项
    std::unordered_map<common::VIndexId, std::unique_ptr<entry::VectorIndexEntry>> vector_indexes_;  ///< 向量索引项
    MetaStore * store_ {nullptr};                                                       ///< 元数据存储，可为空
};

} // namespace litedb::core::meta
