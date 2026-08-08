#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// 创建向量索引方法
enum class CreateVectorIndexMethod
{
    Hnsw,                     // HNSW
};

// 向量索引指标
enum class VectorIndexMetric
{
    Default,                  // 默认
    L2,                       // L2
    InnerProduct,             // InnerProduct
    Cosine,                   // Cosine
};

// 向量索引选项
struct VectorIndexOptions
{
    VectorIndexMetric metric {VectorIndexMetric::Default};   // 指标
    std::optional<std::size_t> max_neighbors;                // 最大邻居数
    std::optional<std::size_t> ef_construction;              // 构建时邻居数
    std::optional<std::size_t> ef_search;                    // 搜索时邻居数
    std::optional<std::size_t> random_seed;                  // 随机种子
};

// 创建向量索引语句节点
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
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取索引名称
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 获取列名称
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    // 是否存在 IF NOT EXISTS
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    // 获取创建向量索引方法
    [[nodiscard]]
    CreateVectorIndexMethod method() const noexcept;

    // 获取向量索引选项
    [[nodiscard]]
    const VectorIndexOptions & options() const noexcept;

private:
    std::string index_name_;                      // 索引名称
    std::string collection_name_;                 // 集合名称
    std::string column_name_;                     // 列名称
    bool if_not_exists_;                          // 是否存在 IF NOT EXISTS
    CreateVectorIndexMethod method_;              // 创建向量索引方法
    VectorIndexOptions options_;                  // 向量索引选项
};

} // namespace litedb::core::parser::ast
