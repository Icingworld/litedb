#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/index/scalar_index.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 索引查找类型
 */
enum class IndexLookupKind
{
    Equal,                ///< 等于
    Range,                ///< 范围
};

/**
 * @brief 索引查找
 */
struct IndexLookup
{
    IndexLookupKind kind;       ///< 索引查找类型
    index::ScalarIndexKey key;  ///< 索引键
    index::IndexRange range;    ///< 索引范围

    [[nodiscard]]
    static IndexLookup equal(index::ScalarIndexKey key);

    [[nodiscard]]
    static IndexLookup range_scan(index::IndexRange range);

private:
    IndexLookup(IndexLookupKind kind, index::ScalarIndexKey key, index::IndexRange range);
};

/**
 * @brief 逻辑索引扫描
 */
class LogicalIndexScan final : public LogicalPlanNode
{
public:
    LogicalIndexScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::IndexId index_id,
        index::IndexKind index_kind,
        common::ColumnId column_id,
        IndexLookup lookup,
        parser::ast::AstNodeLocation location
    );

public:
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    [[nodiscard]]
    index::IndexKind index_kind() const noexcept;

    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    [[nodiscard]]
    const IndexLookup & lookup() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    common::IndexId index_id_;
    index::IndexKind index_kind_;
    common::ColumnId column_id_;
    IndexLookup lookup_;
};

} // namespace litedb::core::planner::logical
