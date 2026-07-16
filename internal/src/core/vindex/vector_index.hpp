#pragma once

#include <cstddef>
#include <expected>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/vector_index_error.hpp"
#include "core/vindex/vector_index_key.hpp"

namespace litedb::core::vindex
{

/**
 * @brief 向量索引类型
 */
enum class VectorIndexKind
{
    Flat,   ///< 精确向量索引
    Hnsw,   ///< HNSW 近似最近邻索引
};

/**
 * @brief 向量距离度量
 */
enum class VectorDistanceMetric
{
    L2,             ///< 欧氏距离
    InnerProduct,   ///< 内积，值越小代表越近时会取负内积
    Cosine,         ///< 余弦距离
};

/**
 * @brief 向量搜索参数
 */
struct VectorSearchRequest
{
    std::size_t top_k {10}; ///< 返回数量
};

/**
 * @brief 向量搜索结果
 */
struct VectorSearchResult
{
    common::RecordId record_id; ///< 记录 ID
    double distance {0.0};      ///< 距离
};

/**
 * @brief 向量索引
 */
class VectorIndex
{
public:
    virtual ~VectorIndex() noexcept = default;

public:
    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    virtual VectorIndexKind kind() const noexcept = 0;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    virtual VectorDistanceMetric metric() const noexcept = 0;

    /**
     * @brief 获取向量维度
     * @return 向量维度
     */
    [[nodiscard]]
    virtual std::size_t dimension() const noexcept = 0;

    /**
     * @brief 插入向量
     * @param key 向量索引键
     * @param record_id 记录 ID
     * @return 结果
     */
    virtual std::expected<void, VectorIndexError> insert(
        const VectorIndexKey & key,
        common::RecordId record_id
    ) = 0;

    /**
     * @brief 删除向量
     * @param record_id 记录 ID
     * @return 结果
     */
    virtual std::expected<void, VectorIndexError> erase(common::RecordId record_id) = 0;

    /**
     * @brief 搜索最近邻
     * @param query 查询向量
     * @param request 搜索请求
     * @return 搜索结果
     */
    [[nodiscard]]
    virtual std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const = 0;

    /**
     * @brief 获取索引大小
     * @return 向量数量
     */
    [[nodiscard]]
    virtual std::size_t size() const noexcept = 0;
};

} // namespace litedb::core::vindex
