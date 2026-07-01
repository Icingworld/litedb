#pragma once

#include <cstddef>
#include <string>

#include "core/catalog/catalog_entry.hpp"
#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief CREATE VINDEX 语句计划
 */
class CreateVectorIndexPlan final : public StatementPlan
{
public:
    CreateVectorIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        std::string index_name,
        catalog::CatalogVectorIndexKind index_kind,
        catalog::CatalogVectorDistanceMetric metric,
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
    catalog::CatalogVectorIndexKind index_kind() const noexcept;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    catalog::CatalogVectorDistanceMetric metric() const noexcept;

    /**
     * @brief 获取最大邻居数
     * @return 最大邻居数
     */
    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    /**
     * @brief 获取构建索引时使用的近似邻居数
     * @return 构建索引时使用的近似邻居数
     */
    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    /**
     * @brief 获取搜索时使用的近似邻居数
     * @return 搜索时使用的近似邻居数
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

private:
    common::DatabaseId database_id_;                    ///< 数据库 ID
    common::CollectionId collection_id_;                ///< 集合 ID
    std::string collection_name_;                       ///< 集合名称
    common::ColumnId column_id_;                        ///< 列 ID
    std::string column_name_;                           ///< 列名称
    std::string index_name_;                            ///< 索引名称
    catalog::CatalogVectorIndexKind index_kind_;        ///< 索引类型
    catalog::CatalogVectorDistanceMetric metric_;       ///< 距离度量
    std::size_t max_neighbors_;                         ///< 最大邻居数
    std::size_t ef_construction_;                       ///< 构建索引时使用的近似邻居数
    std::size_t ef_search_default_;                     ///< 搜索时使用的近似邻居数
    std::size_t random_seed_;                           ///< 随机种子
    bool if_not_exists_;                                ///< 是否存在
};

} // namespace litedb::core::planner::plan
