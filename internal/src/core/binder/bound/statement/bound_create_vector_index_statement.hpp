#pragma once

#include <cstddef>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/meta/meta.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief CREATE VINDEX 语句节点
 * @details 示例：CREATE VINDEX [IF NOT EXISTS] <index_name> ON <collection_name> (<column_name>)
 */
class BoundCreateVectorIndexStatement final : public BoundStatement
{
public:
    BoundCreateVectorIndexStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        std::string index_name,
        meta::entry::VectorIndexKind index_kind,
        meta::entry::VectorDistanceMetric metric,
        std::size_t max_neighbors,
        std::size_t ef_construction,
        std::size_t ef_search_default,
        std::size_t random_seed,
        bool if_not_exists,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取列名称
     * @return 列名称
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    /**
     * @brief 获取索引名称
     * @return 索引名称
     */
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    meta::entry::VectorIndexKind index_kind() const noexcept;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    meta::entry::VectorDistanceMetric metric() const noexcept;

    /**
     * @brief 获取最大邻居数
     * @return 最大邻居数
     */
    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    /**
     * @brief 获取构建时 EF 值
     * @return 构建时 EF 值
     */
    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    /**
     * @brief 获取搜索时默认 EF 值
     * @return 搜索时默认 EF 值
     */
    [[nodiscard]]
    std::size_t ef_search_default() const noexcept;

    /**
     * @brief 获取随机种子
     * @return 随机种子
     */
    [[nodiscard]]
    std::size_t random_seed() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundStatementVisitor & visitor) const override;

private:
    common::DatabaseId database_id_;                    ///< 数据库 ID
    common::CollectionId collection_id_;                ///< 集合 ID
    std::string collection_name_;                       ///< 集合名称
    common::ColumnId column_id_;                        ///< 列 ID
    std::string column_name_;                           ///< 列名称
    std::string index_name_;                            ///< 索引名称
    meta::entry::VectorIndexKind index_kind_;        ///< 索引类型
    meta::entry::VectorDistanceMetric metric_;       ///< 距离度量
    std::size_t max_neighbors_;                         ///< 最大邻居数
    std::size_t ef_construction_;                       ///< 构建时 EF 值
    std::size_t ef_search_default_;                     ///< 搜索时默认 EF 值
    std::size_t random_seed_;                           ///< 随机种子
    bool if_not_exists_;                                ///< 是否存在
};

} // namespace litedb::core::binder::bound
