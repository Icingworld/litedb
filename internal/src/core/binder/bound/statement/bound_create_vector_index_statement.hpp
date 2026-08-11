#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"

namespace litedb::core::binder::bound
{

// 绑定 CREATE VINDEX 语句
class BoundCreateVectorIndexStatement final : public BoundStatement
{
public:
    BoundCreateVectorIndexStatement(
        common::ColumnId column_id,
        std::optional<std::string> vector_index_name,
        meta::entry::VectorIndexKind vector_index_kind,
        meta::entry::VectorDistanceMetric metric,
        std::size_t max_neighbors,
        std::size_t ef_construction,
        std::size_t ef_search_default,
        std::size_t random_seed
    );

public:
    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取索引名称
    [[nodiscard]]
    std::optional<const std::string &> vector_index_name() const noexcept;

    // 获取索引名称所有权
    // 调用后 vector_index_name() 返回 nullopt；再次调用返回 nullopt
    [[nodiscard]]
    std::optional<std::string> take_vector_index_name() noexcept;

    // 获取索引类型
    [[nodiscard]]
    meta::entry::VectorIndexKind vector_index_kind() const noexcept;

    // 获取距离度量
    [[nodiscard]]
    meta::entry::VectorDistanceMetric metric() const noexcept;

    // 获取最大邻居数
    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    // 获取构建时 EF 值
    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    // 获取搜索时默认 EF 值
    [[nodiscard]]
    std::size_t ef_search_default() const noexcept;

    // 获取随机种子
    [[nodiscard]]
    std::size_t random_seed() const noexcept;

private:
    common::ColumnId column_id_;
    // vector_index_name_ 为 nullopt 时表示用户传入了重复向量索引名但是用了 IF NOT EXISTS
    std::optional<std::string> vector_index_name_;
    meta::entry::VectorIndexKind vector_index_kind_;
    meta::entry::VectorDistanceMetric metric_;
    std::size_t max_neighbors_;
    std::size_t ef_construction_;
    std::size_t ef_search_default_;
    std::size_t random_seed_;
};

} // namespace litedb::core::binder::bound
