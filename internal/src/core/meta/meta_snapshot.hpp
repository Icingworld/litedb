#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/meta/entry/default_expression.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"

namespace litedb::core::meta
{

/**
 * @brief 列快照
 */
struct MetaSnapshotColumn
{
    common::ColumnId id {0};                                        ///< 列ID
    std::string name;                                               ///< 列名
    common::LogicalType type;                                       ///< 列类型
    bool unique {false};                                            ///< 是否唯一
    bool nullable {true};                                           ///< 是否可空
    std::optional<entry::DefaultExpression> default_expression;     ///< 默认值表达式
    std::optional<std::string> comment;                             ///< 列注释
};

/**
 * @brief 索引快照
 */
struct MetaSnapshotIndex
{
    common::IndexId id {0};                                         ///< 索引ID
    std::vector<common::ColumnId> column_ids;                       ///< 列 ID 列表
    std::string name;                                               ///< 索引名
    entry::IndexKind index_kind {entry::IndexKind::BTree};          ///< 索引类型
    bool unique {false};                                            ///< 是否唯一
};

/**
 * @brief 向量索引快照
 */
struct MetaSnapshotVectorIndex
{
    common::VIndexId id {0};                                                ///< 向量索引 ID
    common::ColumnId column_id {0};                                         ///< 列 ID
    std::string name;                                                       ///< 向量索引名
    entry::VectorIndexKind index_kind {entry::VectorIndexKind::Hnsw};       ///< 向量索引类型
    entry::VectorDistanceMetric metric {entry::VectorDistanceMetric::L2};   ///< 距离度量
    std::size_t dimension {0};                                              ///< 向量维度
    std::size_t max_neighbors {16};                                         ///< 最大邻居数量
    std::size_t ef_construction {200};                                      ///< 构建候选数量
    std::size_t ef_search_default {64};                                     ///< 默认搜索候选数量
    std::size_t random_seed {0};                                            ///< 随机种子
};

/**
 * @brief 集合快照
 */
struct MetaSnapshotCollection
{
    common::CollectionId id {0};                                    ///< 集合 ID
    common::DatabaseId database_id {0};                             ///< 数据库 ID
    std::string name;                                               ///< 集合名
    std::optional<std::string> comment;                             ///< 集合注释
    std::vector<MetaSnapshotColumn> columns;                        ///< 列列表
    std::vector<MetaSnapshotIndex> indexes;                         ///< 索引列表
    std::vector<MetaSnapshotVectorIndex> vector_indexes;            ///< 向量索引列表
};

/**
 * @brief 数据库快照
 */
struct MetaSnapshotDatabase
{
    common::DatabaseId id {0};                                      ///< 数据库 ID
    std::string name;                                               ///< 数据库名
    std::vector<MetaSnapshotCollection> collections;                ///< 集合列表
};

/**
 * @brief 元数据快照
 */
struct MetaSnapshot
{
    common::DatabaseId next_database_id {1};                        ///< 下一个数据库 ID
    common::CollectionId next_collection_id {1};                    ///< 下一个集合 ID
    common::ColumnId next_column_id {1};                            ///< 下一个列 ID
    common::IndexId next_index_id {1};                              ///< 下一个索引 ID
    common::VIndexId next_vector_index_id {1};                      ///< 下一个向量索引 ID
    std::vector<MetaSnapshotDatabase> databases;                    ///< 数据库列表
};

} // namespace litedb::core::meta
