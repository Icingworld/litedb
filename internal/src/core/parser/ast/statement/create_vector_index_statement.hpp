#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 创建向量索引方法
 */
enum class CreateVectorIndexMethod
{
    Hnsw,                     // HNSW
};

/**
 * @brief 向量索引指标
 */
enum class VectorIndexMetric
{
    Default,                  // 默认
    L2,                       // L2
    InnerProduct,             // InnerProduct
    Cosine,                   // Cosine
};

/**
 * @brief 向量索引选项
 */
struct VectorIndexOptions
{
    VectorIndexMetric metric {VectorIndexMetric::Default};   // 指标
    std::optional<std::size_t> max_neighbors;                // 最大邻居数
    std::optional<std::size_t> ef_construction;              // 构建时邻居数
    std::optional<std::size_t> ef_search;                    // 搜索时邻居数
    std::optional<std::size_t> random_seed;                  // 随机种子
};

/**
 * @brief 创建向量索引语句节点
 */
class CreateVectorIndexStatement final : public StatementNode
{
public:
    CreateVectorIndexStatement(
        std::string index_name,
        std::string collection_name,
        std::string column_name,
        bool if_not_exists,
        CreateVectorIndexMethod method,
        VectorIndexOptions options,
        AstNodeLocation location
    ) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 获取索引名称
     * @return 索引名称
     */
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 获取列名称
     * @return 列名称
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    /**
     * @brief 是否不存在
     * @return 是否不存在
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    /**
     * @brief 获取创建向量索引方法
     * @return 创建向量索引方法
     */
    [[nodiscard]]
    CreateVectorIndexMethod method() const noexcept;

    /**
     * @brief 获取向量索引选项
     * @return 向量索引选项
     */
    [[nodiscard]]
    const VectorIndexOptions & options() const noexcept;

private:
    std::string index_name_;                      // 索引名称
    std::string collection_name_;                 // 集合名称
    std::string column_name_;                     // 列名称
    bool if_not_exists_;                          // 是否不存在
    CreateVectorIndexMethod method_;              // 创建向量索引方法
    VectorIndexOptions options_;                  // 向量索引选项
};

} // namespace litedb::core::parser::ast
