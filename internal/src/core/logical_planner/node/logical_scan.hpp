#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/meta/meta.hpp"
#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/logical_planner/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

enum class LogicalIndexLookupKind
{
    Equal,
    Range,
};

struct LogicalIndexBound
{
    index::ScalarIndexKey key;
    bool inclusive {true};
};

struct LogicalIndexLookup
{
    LogicalIndexLookupKind kind {LogicalIndexLookupKind::Equal};
    std::optional<LogicalIndexBound> lower;
    std::optional<LogicalIndexBound> upper;
};

struct LogicalScanIndexHint
{
    common::IndexId index_id;
    std::string index_name;
    meta::entry::IndexKind index_kind;
    common::ColumnId column_id;
    std::string column_name;
    LogicalIndexLookup lookup;
};

/**
 * @brief 逻辑扫描节点
 * @details 叶子节点，没有子节点，直接继承自 LogicalPlanNode
 */
class LogicalScan final : public LogicalPlanNode
{
public:
    LogicalScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::optional<LogicalScanIndexHint> index_hint,
        parser::ast::AstNodeLocation location
    );

    LogicalScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合ID
     * @return 集合ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    const std::optional<LogicalScanIndexHint> & index_hint() const noexcept;

    /**
     * @brief 接受访问器
     * @param visitor 访问器
     */
    void accept(LogicalPlanNodeVisitor & visitor) const override;

    /**
     * @brief 深拷贝逻辑计划节点
     * @return 逻辑计划节点副本
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> clone() const override;

private:
    common::DatabaseId database_id_;                 ///< 数据库 ID
    common::CollectionId collection_id_;             ///< 集合 ID
    std::string collection_name_;                    ///< 集合名称
    std::optional<LogicalScanIndexHint> index_hint_; ///< index access hint
};

} // namespace litedb::core::planner::logical
