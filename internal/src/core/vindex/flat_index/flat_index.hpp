#pragma once

#include <cstddef>
#include <expected>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::storage
{

class StorageEngine;

} // namespace litedb::core::storage

namespace litedb::core::vindex
{

/**
 * @brief Flat 向量索引配置
 */
struct FlatIndexOptions
{
    common::CollectionId collection_id;                          // 集合 ID
    std::size_t column_ordinal {0};                              // 向量列序号
    std::size_t dimension {0};                                   // 向量维度
    VectorDistanceMetric metric {VectorDistanceMetric::L2};      // 距离度量
};

/**
 * @brief 基于存储全表扫描的精确向量索引
 * @details 不物化任何向量条目，搜索时直接扫描集合中的向量列。
 */
class FlatIndex final : public VectorIndex
{
public:
    FlatIndex(FlatIndexOptions options, const storage::StorageEngine & storage) noexcept;

public:
    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    VectorIndexKind kind() const noexcept override;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    VectorDistanceMetric metric() const noexcept override;

    /**
     * @brief 获取向量维度
     * @return 向量维度
     */
    [[nodiscard]]
    std::size_t dimension() const noexcept override;

    /**
     * @brief 插入向量索引
     * @param key 向量索引键
     * @param record_id 记录 ID
     * @return 结果
     */
    std::expected<void, VectorIndexError> insert(
        const VectorIndexKey & key,
        common::RecordId record_id
    ) override;

    /**
     * @brief 删除向量索引
     * @param record_id 记录 ID
     * @return 结果
     */
    std::expected<void, VectorIndexError> erase(common::RecordId record_id) override;

    /**
     * @brief 搜索向量索引
     * @param query 查询向量
     * @param request 搜索请求
     * @return 搜索结果
     */
    [[nodiscard]]
    std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const override;

    /**
     * @brief 获取物化索引条目数
     * @return Flat 不物化条目，始终返回 0
     */
    [[nodiscard]]
    std::size_t size() const noexcept override;

    /**
     * @brief 获取配置
     * @return 配置
     */
    [[nodiscard]]
    const FlatIndexOptions & options() const noexcept;

private:
    /**
     * @brief 验证向量索引键
     * @param key 向量索引键
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, VectorIndexError> validate_key(const VectorIndexKey & key) const;

private:
    FlatIndexOptions options_;                             // 配置
    const storage::StorageEngine * storage_ {nullptr};     // 非拥有型存储引擎
};

} // namespace litedb::core::vindex
