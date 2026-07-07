#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/catalog/catalog_entry.hpp"
#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/parser/ast/ast_node.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 索引扫描的查询类型
 */
enum class LogicalIndexLookupKind
{
    Equal,
    Range,
};

/**
 * @brief 索引扫描的边界
 */
struct LogicalIndexBound
{
    index::ScalarIndexKey key;
    bool inclusive {true};
};

/**
 * @brief 索引扫描的查询条件
 */
struct LogicalIndexLookup
{
    LogicalIndexLookupKind kind {LogicalIndexLookupKind::Equal};
    std::optional<LogicalIndexBound> lower;
    std::optional<LogicalIndexBound> upper;
};

/**
 * @brief 索引扫描节点
 */
class LogicalIndexScan final : public LogicalPlanNode
{
public:
    LogicalIndexScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::IndexId index_id,
        std::string index_name,
        catalog::CatalogIndexKind index_kind,
        common::ColumnId column_id,
        std::string column_name,
        LogicalIndexLookup lookup,
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
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    common::IndexId index_id() const noexcept;

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
    catalog::CatalogIndexKind index_kind() const noexcept;

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
     * @brief 获取查询条件
     * @return 查询条件
     */
    [[nodiscard]]
    const LogicalIndexLookup & lookup() const noexcept;

    /**
     * @brief 接受访问者
     * @param visitor 访问者
     */
    void accept(LogicalPlanNodeVisitor & visitor) const override;

    /**
     * @brief 克隆节点
     * @return 克隆节点
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> clone() const override;

private:
    common::DatabaseId database_id_;            ///< 数据库 ID
    common::CollectionId collection_id_;        ///< 集合 ID
    std::string collection_name_;               ///< 集合名称
    common::IndexId index_id_;                  ///< 索引 ID
    std::string index_name_;                    ///< 索引名称
    catalog::CatalogIndexKind index_kind_;      ///< 索引类型
    common::ColumnId column_id_;                ///< 列 ID
    std::string column_name_;                   ///< 列名称
    LogicalIndexLookup lookup_;                 ///< 查询条件
};

} // namespace litedb::core::planner::logical
